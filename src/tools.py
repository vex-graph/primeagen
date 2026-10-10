"""Small personal tool surface, not an unrestricted shell.

Read is workspace-confined; Func build and artifact run are separate approvals.
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
  }

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Constructors:
  - Tools(workspace, approve, func, timeout)
Core operations:
  - path(name) : reject escaped, nonexistent and aliased/symlink read targets
  - execute(name, arguments) : exact read/func_build/func_run admission + consent
Lifecycle:
  - close() : reclaim owned artifact directories after child borrows end

Denial performs no operation. Build never runs; run requires a fresh approval and
a session-owned artifact. Malformed args/unknown tools reject before consent.
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
ROOT = Path(__file__).resolve().parent.parent


class Tools:
    """Host-owned policy/configuration; exact args, observable denial/failure."""
    def __init__(self, workspace, approve, func=None, timeout=60.0):
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
        expected = {"read": {"path"}, "func_build": {"source", "syntax"}, "func_run": {"artifact"}}
        if name not in expected or not isinstance(arguments, dict) or set(arguments) != expected[name]:
            raise ValueError("unknown tool or invalid arguments")
        if name == "read":
            path = self.path(arguments["path"])
            if not path.is_file():
                raise ValueError("read requires a regular file")
            if not self.approve(name, arguments):
                return "denied by user"
            with path.open("rb") as stream:
                data = stream.read(FILE_CAP + 1)
            if len(data) > FILE_CAP:
                raise ValueError("file read would truncate")
            return data.decode("utf-8")
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
