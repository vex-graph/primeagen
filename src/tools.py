"""Host-consented native tool surface plus independent Func build/run.

File tools are workspace-confined; approved Bash is expressly NOT sandboxed.
Paths are checked against a stable trusted workspace; this is not a sandbox or
TOCTOU defence against another writer. Compiled artifacts are trusted-code runs.
"""
"""
============================================================================
CLASS: Tools (src/tools.py)
============================================================================
INSTANCE FIELDS (in construction order):
----------------------------------------------------------------------------
  Tools {
    workspace;      // canonical trusted root; all reads must stay beneath it
    approve;        // borrowed host callback; model cannot change consent
    func;           // trusted independently supplied Func launcher path
    timeout;        // caller-selected child-stage deadline
    artifacts;      // flat owned identity-to-private-build-directory registry
     next_artifact;  // monotonically increasing session-local artifact identity
     native;         // host-selected native tools executable, never model-provided
     websearch;      // optional trusted search executable; no default scraper
     byte_cap;       // current bounded file/tool output budget
  }

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Constructors:
  - Tools(workspace, approve, func, timeout, native, websearch, byte_cap)
Core operations:
  - path(name) : reject escaped, nonexistent and aliased/symlink read targets
  - execute(name, arguments) : exact tool-tag/schema admission + consent
Lifecycle:
  - close() : reclaim owned artifact directories after child borrows end

Denial performs no operation. Build never runs; run requires a fresh approval and
 a session-owned artifact. Explicit Bash/search children are separate approvals;
 Bash can escape the workspace and inherits host privileges/environment.
 Malformed args/unknown tools reject before consent. Native file mutations use
 C tools/*.c pairs; Python owns validation, consent and bounded child supervision.
Oversized reads reject rather than silently truncate. Failed builds release their
private directory and do not issue identities. Completed outputs remain owned
until close. Stable-tree checks are not a hostile-directory or process sandbox.
============================================================================
"""
import os
import math
from pathlib import Path
import sys

from process import run

FILE_CAP = 128 * 1024
SOURCE_CAP = 128 * 1024
TOOL_BYTES_MAX = 16 * 1024 * 1024
TOOL_SCHEMAS = {
    "read": {"path"}, "list": {"path"}, "write": {"path", "text"},
    "create": {"path", "text"}, "edit": {"path", "old", "new"},
    "delete": {"path"}, "bash": {"command"}, "websearch": {"query"},
    "func_build": {"source", "syntax"}, "func_run": {"artifact"},
}
ROOT = Path(__file__).resolve().parent.parent


class Tools:
    """Host-owned policy/configuration; exact args, observable denial/failure."""
    def __init__(self, workspace, approve, func=None, timeout=60.0, native=None, websearch=None, byte_cap=FILE_CAP):
        if not math.isfinite(timeout) or not 0 < timeout <= 3600:
            raise ValueError("invalid tool deadline")
        self.workspace = Path(workspace).resolve(strict=True)
        if not self.workspace.is_dir() or not callable(approve):
            raise ValueError("invalid workspace or approval callback")
        self.approve = approve
        self.func = Path(func or os.environ.get("FUNC_PATH", ROOT.parent / "func/func")).absolute()
        self.timeout = timeout
        self.artifacts = {}
        self.next_artifact = 1
        self.native = Path(native or ROOT / ".build/tools").absolute()
        self.websearch = Path(websearch).resolve(strict=True) if websearch else None
        if type(byte_cap) is not int or not 0 < byte_cap <= TOOL_BYTES_MAX:
            raise ValueError("invalid tool byte budget")
        self.byte_cap = byte_cap

    def path(self, name):
        """Reject escape/symlink aliases; stable tree required during the operation."""
        if not isinstance(name, str) or not name or "\0" in name:
            raise ValueError("invalid path")
        candidate = self.workspace / name
        resolved = candidate.resolve(strict=True)
        if not resolved.is_relative_to(self.workspace) or candidate.absolute() != resolved:
            raise ValueError("path escapes workspace or contains a symlink/noncanonical hop")
        return resolved

    def execute(self, name, arguments):
        """No model-provided authorization flag or arbitrary executable is accepted."""
        if name not in TOOL_SCHEMAS or not isinstance(arguments, dict) or set(arguments) != TOOL_SCHEMAS[name]:
            raise ValueError("unknown tool or invalid arguments")
        if name not in {"func_build", "func_run"}:
            if not all(isinstance(value, str) and "\0" not in value and len(value.encode()) <= self.byte_cap
                       for value in arguments.values()):
                raise ValueError("invalid native tool argument")
            if name in {"read", "write", "edit", "delete", "list", "create"}:
                relative = arguments["path"]
                if not relative or relative.startswith("/") or any(ord(c) < 32 or ord(c) == 127 for c in relative):
                    raise ValueError("invalid relative path")
                if relative != "." and any(part in {"", ".", ".."} for part in relative.split("/")):
                    raise ValueError("noncanonical relative path")
                if name == "create":
                    target = self.workspace / relative
                    self.path(str(target.parent.relative_to(self.workspace)) or ".")
                    if target.exists() or target.is_symlink() or relative == ".":
                        raise ValueError("create requires an absent file")
                else:
                    path = self.path(relative)
                    if name == "list":
                        if not path.is_dir():
                            raise ValueError("list requires a directory")
                    elif not path.is_file() or path.stat().st_nlink != 1:
                        raise ValueError("tool requires regular single-link file")
            if name == "edit" and not arguments["old"]:
                raise ValueError("edit requires nonempty old text")
            if name == "bash" and not arguments["command"]:
                raise ValueError("bash requires a nonempty command")
            if name == "websearch" and (not arguments["query"] or self.websearch is None or not self.websearch.is_file()):
                raise ValueError("websearch requires a configured trusted search executable (--websearch-tool)")
            if not self.native.is_file():
                raise ValueError("native tools missing; build explicitly with sh tools/build.sh")
            if not self.approve(name, arguments):
                return "denied by user"
            argv = [str(self.native), str(self.workspace), str(self.byte_cap), name]
            data = b""
            if "path" in arguments:
                argv.append(arguments["path"])
            if name in {"write", "create"}:
                data = arguments["text"].encode("utf-8")
            elif name == "edit":
                argv += [arguments["old"], arguments["new"]]
            elif name == "bash":
                argv.append(arguments["command"])
            elif name == "websearch":
                argv += [str(self.websearch), arguments["query"]]
            try:
                return run(argv, data=data, timeout=self.timeout, cap=self.byte_cap).decode("utf-8")
            except (ValueError, TimeoutError) as error:
                cleanup = "; process-group cleanup refused; descendants unresolved" if "process-group cleanup refused" in str(error) else ""
                raise ValueError(f"native {name} rejected, failed, timed out or would truncate; completed side effects are not rolled back{cleanup}") from error
        if name == "func_build":
            source, syntax = arguments["source"], arguments["syntax"]
            if not isinstance(source, str) or not source or len(source.encode()) > SOURCE_CAP or syntax not in ("numeric", "surface"):
                raise ValueError("invalid Func source or syntax")
            if not self.func.is_file():
                raise ValueError("Func executable missing; set FUNC_PATH")
            if not self.approve(name, arguments):
                return "denied by user"
            import tempfile
            # Private per-build resources outlive run borrows and are removed at close.
            home = tempfile.TemporaryDirectory(prefix="primeagen-func-")
            try:
                path = Path(home.name)
                src = path / "program.func"
                src.write_text(source)
                artifact = path / "artifact"
                run([sys.executable, str(self.func), "build", str(src), "--syntax", syntax,
                     "-o", str(artifact), "--timeout", str(self.timeout)], timeout=self.timeout * 3 + 5)
                identity = str(self.next_artifact)
                self.next_artifact += 1
                self.artifacts[identity] = home
                return "built artifact " + identity + "; not executed"
            except BaseException:
                home.cleanup()
                raise
        identity = arguments["artifact"]
        if not isinstance(identity, str) or identity not in self.artifacts:
            raise ValueError("unknown session-owned artifact")
        if not self.approve(name, arguments):
            return "denied by user"
        home = self.artifacts[identity]
        return run([str(Path(home.name) / "artifact")], timeout=self.timeout).decode()

    def close(self):
        """Detach all artifact borrows before reclaiming private build state."""
        for home in self.artifacts.values():
            home.cleanup()
        self.artifacts.clear()
