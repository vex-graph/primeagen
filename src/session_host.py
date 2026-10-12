"""Optional OpenCode V2 session host for the native Primeagen terminal.

============================================================================
MODULE: Primeagen OpenCode host (src/session_host.py)
============================================================================
OWNED STATE:
  Launcher owns two pipes and terminal/worker children. Worker retains one newly
  created session ID and one in-flight turn record; OpenCode owns login/config/
  model state, history, remote execution and any saved project approval. No
  credential files or .env are read here.
FUNCTION REGISTRY:
  api(method, path, body, timeout, empty) : bounded literal OpenCode CLI API
      invocation; reject diagnostics, malformed/oversized/deep/duplicate JSON.
  checked_id(value) : validate remote session ID before embedding in a path.
  checked_request(value) : validate remote permission-request ID before use.
  matches(pattern, value) : whole-value wildcard comparison used by policy eval.
  effective(rules, action, resource) : last-matching-rule-wins policy evaluation.
  confirmed(echo) : refuse a reported policy that does not match what we asked.
  create(model) : create the dedicated session under the default policy.
  inspect(session) : confirm the enforced permission ruleset is unchanged.
  request(session) : first pending permission request, bounded and validated.
  label(item) : bounded printable display text for one request.
  decide(session, target, decision) : reply once/always/reject to one request.
  begin(session, prompt, instructions) : admit one prompt under a turn deadline.
  collect(item, answers, tools) : project one assistant message, bounded.
  parts(item) : ordered, validated display parts of one assistant message.
  narration(text) : bounded single-line prose notes.
  clip(value, cap) : one bounded printable fragment from remote text.
  pathOf(arguments) : bounded target path from a tool call's arguments.
  diffLines(old, new, cap) : bounded common-prefix/suffix line diff.
  summarize(name, arguments) : one tool call as bounded transcript lines.
  stream(turn, item, items, final, notes) : append a message's new parts in order;
      thinking, tool lines and prose stream immediately, while the newest message's
      prose is held so the turn ends on its conclusion.
  advance(session, turn) : one bounded step; returns (notes, ask, done). The
      deadline is an idle window restarted by observed remote progress, so a long
      multi-tool turn is bounded by silence, not by total wall time.
  interrupt(session) : best-effort remote cancellation, never local proof.
  emit(payload) : one bounded newline JSON frame to the native terminal.
  worker(model) : UTF-8 newline JSON frame loop; final answers, approval requests
      and generic failures. UTF-16 JSON escapes are decoded by Python, then emitted
      as scalar UTF-8; no surrogate halves cross the native parser/display boundary.
  terminate(signum, frame) : raise cancellation so bounded child cleanup executes.
  launch(owo, model) : pass nonblocking wire fds to the native TUI; the terminal
      owns display; on every exit terminate/kill/reap worker, close pipes.
  main() : explicit --engine opencode, optional provider/model, --owo or private
      --worker entry. Nonterminal launch rejects before starting any worker.
CONTRACT:
  Trusted installed OpenCode V2 executable/server; subscriptions remain with its
  login. Model is explicit provider/id when selected, otherwise OpenCode default.
  Never attaches to an existing session or this harness conversation. The default
  session policy is automatic for inspection and asks the terminal before edits,
  shell, web fetch/search, external directories and `.env` reads; every other
  action is denied. Only one bounded request is presented at a time; the queue is
  re-read after each decision. A saved "always" approval is OpenCode's durable
  project rule and a configured deny always wins. Tool execution is OpenCode's,
  under that enforced ruleset; the local C tool adapters are not involved here.
  Session history stays on OpenCode server after exit; not ephemeral/private RAM.
  Successful remote admission can incur subscription use/charges. No token copying,
  automatic installs, project config changes, direct vendor API or Codex support.
  The whole-turn deadline is an idle window that excludes human decision time and
  restarts on remote progress; the message page keeps its own bounded read budget
  separate from the frame caps. Quiet generic failure. Remote interrupt is best
  effort; server work may outlive local timeout. Poll cadence is idle IO
  scheduling, not test synchronization or frame cadence. POSIX/macOS only; no
  native-agent, stream-delta or sandbox claim.
============================================================================
"""
import argparse
import json
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import sys
import time
import uuid

from json_input import loads
from process import run

ROOT = Path(__file__).resolve().parent.parent
TURN_TIMEOUT = 300.0  # Idle window: no observed remote progress for this long.
MESSAGE_TIMEOUT = 25.0  # One message-page read; a slow page is retried, not fatal.
WIRE_CAP = 65536
# A tool-using turn's newest page carries tool arguments and content, so the
# message read gets its own bounded budget rather than the frame cap. It stays
# finite: the page is parsed as untrusted JSON, never streamed unbounded.
MESSAGE_CAP = 8 * 1024 * 1024
PAGE_LIMIT = 200  # Reject a single turn exceeding this bounded newest page.
REQUEST_LIMIT = 8  # Reject an implausible pending-approval page.
LABEL_CAP = 200  # Bounded native display text; the C bridge rejects overflow.
NOTE_CAP = 400   # Bounded streamed activity line; below the bridge's 511-byte cap.
LINE_CAP = 160   # Bounded single argument fragment inside one note line.
DIFF_LINE_CAP = 12  # Bounded added/removed lines shown per edit.
NARRATION_LINES = 6  # Bounded prose lines streamed per assistant message.
PERMISSION_POLL = 1.0  # Pending-approval poll cadence, not a turn budget.
DECISIONS = ("once", "always", "reject")
# Effective effects the default policy must produce after evaluation. Inspection
# is automatic; mutation, shell, network and external directories wait for a
# human; a synthetic name nothing else covers must still fall through to deny.
EXPECTED_EFFECTS = {
    "read": "allow", "glob": "allow", "grep": "allow",
    "edit": "ask", "shell": "ask", "webfetch": "ask", "websearch": "ask",
    "external_directory": "ask", "dangerous_unknown_action": "deny",
}
# Default session policy, ordered so the last matching rule wins: the broad deny
# is stated first, then automatic inspection, then the actions a human must
# approve. `.env` reads stay asked even though reads are otherwise automatic.
SESSION_PERMISSIONS = [
    {"action": "*", "resource": "*", "effect": "deny"},
    {"action": "read", "resource": "*", "effect": "allow"},
    {"action": "glob", "resource": "*", "effect": "allow"},
    {"action": "grep", "resource": "*", "effect": "allow"},
    {"action": "read", "resource": "*.env", "effect": "ask"},
    {"action": "read", "resource": "*.env.*", "effect": "ask"},
    {"action": "edit", "resource": "*", "effect": "ask"},
    {"action": "shell", "resource": "*", "effect": "ask"},
    {"action": "webfetch", "resource": "*", "effect": "ask"},
    {"action": "websearch", "resource": "*", "effect": "ask"},
    {"action": "external_directory", "resource": "*", "effect": "ask"},
]


def api(method, path, body=None, timeout=10.0, empty=False, cap=WIRE_CAP, allowStderr=False):
    """Use official CLI discovery/auth, never inspect its token store."""
    argv = ["opencode", "api", method, path]
    if body is not None:
        argv += ["--data", json.dumps(body, ensure_ascii=True, separators=(",", ":"))]
    raw = run(argv, timeout=timeout, cap=cap, allowStderr=allowStderr)
    if empty and not raw.strip():
        return {}
    result = loads(raw)
    if not isinstance(result, dict):
        raise ValueError("invalid API response object")
    return result


def checked_id(value):
    """IDs are not URLs, traversal, command fragments or borrowed session names."""
    if not isinstance(value, str) or not re.fullmatch(r"ses[A-Za-z0-9_]{1,128}", value):
        raise ValueError("invalid remote session identity")
    return value


def matches(pattern, value):
    """Whole-value wildcard: '*' is zero or more characters, '?' is exactly one."""
    expression = "^" + "".join(".*" if character == "*" else "." if character == "?"
                               else re.escape(character) for character in pattern) + "$"
    return re.fullmatch(expression, value) is not None


def effective(rules, action, resource="*"):
    """Evaluate a ruleset the way OpenCode documents it: last matching rule wins."""
    result = "ask"  # OpenCode's default when no rule matches.
    for rule in rules:
        if matches(rule["action"], action) and matches(rule["resource"], resource):
            result = rule["effect"]
    return result


def confirmed(echo):
    """Refuse to run unless the server reports the policy we asked for.

    The echoed ruleset is evaluated exactly as OpenCode evaluates it, so a server
    that reorders, drops or extends the session policy into something more
    permissive is rejected before any prompt is sent. Extra rules are tolerated
    while the effective effects still match; a mismatched effect is not.
    """
    if not isinstance(echo, list) or not echo or len(echo) > 64:
        return False
    for rule in echo:
        if not isinstance(rule, dict) or set(rule) != {"action", "resource", "effect"}:
            return False
        if not isinstance(rule["action"], str) or not isinstance(rule["resource"], str):
            return False
        if not rule["action"] or not rule["resource"] or rule["effect"] not in ("allow", "deny", "ask"):
            return False
    for action, expected in EXPECTED_EFFECTS.items():
        if effective(echo, action) != expected:
            return False
    return effective(echo, "read", "secrets/nested.env") == "ask"


def checked_request(value):
    """Permission identities are validated before any literal API path use."""
    if not isinstance(value, str) or not re.fullmatch(r"per[A-Za-z0-9_]{1,128}", value):
        raise ValueError("invalid remote permission identity")
    return value


def create(model):
    """Admit one dedicated session under the default inspection/mutation policy."""
    body = {"title": "Primeagen chat", "location": {"directory": str(Path.cwd())},
            "permissions": SESSION_PERMISSIONS}
    if model:
        provider, separator, identifier = model.partition("/")
        if not separator or not provider or not identifier or any(ord(c) < 33 for c in model):
            raise ValueError("model must be provider/model")
        body["model"] = {"providerID": provider, "id": identifier}
    info = api("post", "/api/session", body).get("data", {})
    session = checked_id(info.get("id"))
    if not confirmed(info.get("permissions")):
        raise ValueError("server did not confirm the session permission policy")
    return session


def inspect(session):
    """A dedicated session must keep the admitted policy for the whole run."""
    info = api("get", "/api/session/" + session).get("data", {})
    if not confirmed(info.get("permissions")):
        raise ValueError("session permission policy changed")
    return True


def request(session):
    """Return the first pending approval, or None. Never auto-decides."""
    data = api("get", "/api/session/" + session + "/permission").get("data")
    if not isinstance(data, list) or len(data) > REQUEST_LIMIT:
        raise ValueError("invalid pending permission list")
    for item in data:
        if not isinstance(item, dict):
            raise ValueError("invalid pending permission request")
        action = item.get("action")
        resources = item.get("resources")
        if not isinstance(action, str) or not action or len(action) > 64:
            raise ValueError("invalid pending permission action")
        if not isinstance(resources, list) or not resources or len(resources) > REQUEST_LIMIT:
            raise ValueError("invalid pending permission resources")
        if not all(isinstance(value, str) and len(value) <= 512 for value in resources):
            raise ValueError("invalid pending permission resource")
        return {"id": checked_request(item.get("id")), "action": action, "resources": resources}
    return None


def label(item):
    """One bounded line of printable text; remote content never reaches the screen raw."""
    text = item["action"] + ": " + ", ".join(item["resources"])
    safe = "".join(character if 32 <= ord(character) < 127 else " " for character in text)
    return safe[:LABEL_CAP]


def decide(session, target, decision):
    """Forward an explicit human decision; the server owns the durable rule."""
    if decision not in DECISIONS:
        raise ValueError("invalid permission decision")
    api("post", "/api/session/" + session + "/permission/" + target["id"] + "/reply",
        {"decision": decision}, timeout=10.0, empty=True)


def begin(session, prompt, instructions):
    """Admit one prompt; the unique message ID is this turn's exact boundary."""
    message_id = "msg_" + uuid.uuid4().hex
    api("post", "/api/session/" + session + "/prompt", {"id": message_id, "text": instructions + prompt})
    return {"messageId": message_id, "deadline": time.monotonic() + TURN_TIMEOUT,
            "awaiting": None, "awaitingSince": 0.0, "nextCheck": 0.0, "seen": set(),
            "said": set(), "held": [], "flowed": False, "newest": None}


def parts(item):
    """Ordered, validated display parts of one assistant message."""
    if not isinstance(item, dict) or item.get("type") != "assistant":
        return []
    if item.get("error") or item.get("finish") in ("error", "length", "content-filter"):
        raise ValueError("agent turn rejected")
    content = item.get("content", [])
    if not isinstance(content, list) or len(content) > PAGE_LIMIT:
        raise ValueError("invalid assistant content")
    out = []
    for index, part in enumerate(content):
        if not isinstance(part, dict):
            raise ValueError("invalid assistant content part")
        kind = part.get("type")
        if kind == "tool":
            name = part.get("name")
            identity = part.get("id")
            if not isinstance(name, str) or not name or len(name) > 64:
                raise ValueError("invalid tool identity")
            if not isinstance(identity, str) or not identity or len(identity) > 128:
                raise ValueError("invalid tool identity")
            state = part.get("state")
            out.append(("tool", identity, name, state if isinstance(state, dict) else {}))
        elif kind in ("text", "reasoning"):
            text = part.get("text")
            if not isinstance(text, str):
                raise ValueError("invalid assistant text")
            out.append((kind, index, text))
        # Unrecognized part kinds stay ignored rather than guessed at.
    return out


def narration(text):
    """Model prose as bounded single-line notes; empty lines are dropped."""
    return [clip(line) for line in text.splitlines() if line.strip()][:NARRATION_LINES]


def clip(value, cap=LINE_CAP):
    """One bounded printable fragment; remote content never reaches the screen raw."""
    text = "" if value is None else str(value)
    text = "".join(character if 32 <= ord(character) < 127 else " " for character in text).strip()
    return text[:cap] + ("..." if len(text) > cap else "")


def pathOf(arguments):
    for key in ("filePath", "path", "file", "filename"):
        value = arguments.get(key)
        if isinstance(value, str) and value:
            return clip(value)
    return ""


def diffLines(old, new, cap=DIFF_LINE_CAP):
    """Common-prefix/suffix line diff; a bounded list, never a colored painting."""
    oldLines, newLines = old.splitlines(), new.splitlines()
    start = 0
    while start < len(oldLines) and start < len(newLines) and oldLines[start] == newLines[start]:
        start += 1
    end = 0
    while (end < len(oldLines) - start and end < len(newLines) - start
           and oldLines[len(oldLines) - 1 - end] == newLines[len(newLines) - 1 - end]):
        end += 1
    lines = [f"  - {clip(line)}" for line in oldLines[start:len(oldLines) - end][:cap]]
    lines += [f"  + {clip(line)}" for line in newLines[start:len(newLines) - end][:cap]]
    return lines or ["  (no line change)"]


def summarize(name, arguments):
    """One tool call as bounded transcript lines. Display text only, never a
    command, and never a re-execution: OpenCode already ran it."""
    tool = name.strip().lower() or "tool"
    if tool in ("bash", "shell"):
        command = arguments.get("command")
        return [f"shell: {clip(command)}"] if isinstance(command, str) and command.strip() else ["shell:"]
    if tool == "websearch":
        query = arguments.get("query")
        return [f'websearch: "{clip(query)}"'] if isinstance(query, str) else ["websearch:"]
    if tool in ("webfetch", "fetch"):
        url = arguments.get("url")
        return [f"webfetch: {clip(url)}"] if isinstance(url, str) else ["webfetch:"]
    if tool in ("glob", "grep"):
        pattern = arguments.get("pattern")
        return [f'{tool}: "{clip(pattern)}"'] if isinstance(pattern, str) else [f"{tool}:"]
    path = pathOf(arguments)
    if tool in ("edit", "patch", "multiedit"):
        lines = [f"edit: {path or 'file'}"]
        old, new = arguments.get("oldString"), arguments.get("newString")
        if isinstance(old, str) and isinstance(new, str):
            lines.extend(diffLines(old, new))
        return lines
    if tool == "read":
        offset, limit = arguments.get("offset"), arguments.get("limit")
        where = ""
        if isinstance(offset, int) and offset > 0:
            if isinstance(limit, int) and limit > 0:
                where = f" (lines {offset}-{offset + limit - 1})"
            else:
                where = f" (from line {offset})"
        return [f"read: {path or 'file'}{where}"]
    if path:
        return [f"{tool}: {path}"]
    return [f"{tool}:"]


def stream(turn, item, items, final, notes):
    """Append one assistant message's new parts in order: thinking, tool lines and
    prose. The newest message's prose is held as the answer rather than streamed,
    so a turn reads as a flow that ends on its conclusion."""
    identity = item.get("id")
    for part in items:
        kind = part[0]
        if kind == "tool":
            _, key, name, state = part
            if key in turn["seen"]:
                continue
            arguments = state.get("input")
            if state.get("status") not in ("running", "completed", "error") or not isinstance(arguments, dict):
                continue
            turn["seen"].add(key)
            notes.extend(summarize(name, arguments))
        elif kind == "reasoning":
            _, index, text = part
            mark = (identity, "r", index)
            if mark in turn["said"] or not text.strip():
                continue
            turn["said"].add(mark)
            notes.append("thinking: " + clip(text, NOTE_CAP - 12))
        else:
            _, index, text = part
            mark = (identity, "t", index)
            if mark in turn["said"] or not text.strip():
                continue
            if final:
                continue  # held as this turn's conclusion; streamed if superseded
            turn["said"].add(mark)
            notes.extend(narration(text))
    return notes


def advance(session, turn):
    """One bounded progress step -> (notes, ask, done). Never blocks on a human."""
    if turn["awaiting"] is not None:
        return [], None, None
    if time.monotonic() >= turn["deadline"]:
        raise ValueError("remote turn idle window expired")
    # Pending approvals are polled every slice regardless of where this turn's
    # prompt message sits in the page. The remote agent blocks on a permission
    # request wherever it is in the turn, so checking only while the boundary is
    # missing left the request unanswered until the watchdog killed the turn.
    if time.monotonic() >= turn["nextCheck"]:
        turn["nextCheck"] = time.monotonic() + PERMISSION_POLL
        pending = request(session)
        if pending is not None:
            turn["awaiting"] = pending
            return [], pending, None
    try:
        result = api("get", "/api/session/" + session + "/message?order=desc&limit=" + str(PAGE_LIMIT),
                     timeout=MESSAGE_TIMEOUT, cap=MESSAGE_CAP, allowStderr=True)
    except TimeoutError:
        return [], None, None  # a slow page is not a dead turn; retry next slice
    messages = result.get("data")
    if not isinstance(messages, list) or len(messages) > PAGE_LIMIT:
        raise ValueError("invalid session messages")
    # Any new newest message is progress: the idle window restarts, so a long
    # multi-tool turn is bounded by remote silence rather than total wall time.
    if messages and messages[0].get("id") != turn["newest"]:
        turn["newest"] = messages[0].get("id")
        turn["deadline"] = time.monotonic() + TURN_TIMEOUT
    boundary = next((i for i, item in enumerate(messages) if item.get("id") == turn["messageId"]), None)
    if boundary is None:
        if len(messages) == PAGE_LIMIT:
            raise ValueError("turn exceeded response page safety limit")
        return [], None, None
    parsed = [(item, parts(item)) for item in reversed(messages[:boundary])]
    completed = False
    for item, _ in parsed:
        if isinstance(item, dict) and item.get("type") == "idle":
            if item.get("outcome") != "succeeded":
                raise ValueError("remote session did not succeed")
            completed = True
    assistants = [item for item, _ in parsed if isinstance(item, dict) and item.get("type") == "assistant"]
    newest = assistants[-1].get("id") if assistants else None
    notes = []
    turn["held"] = []
    for item, items in parsed:
        if not isinstance(item, dict) or item.get("type") != "assistant":
            continue
        final = item.get("id") == newest
        if final:
            turn["held"] = [part[2] for part in items if part[0] == "text" and part[2].strip()]
        stream(turn, item, items, final, notes)
    if notes:
        turn["flowed"] = True
    if not completed:
        return notes, None, None
    text = "\n".join(turn["held"]).strip()
    if not text and not turn["flowed"]:
        text = "(no text returned)"  # a completed turn may be tool-only or silent
    if len(json.dumps({"ok": True, "text": text}, ensure_ascii=False).encode("utf-8")) >= WIRE_CAP - 1:
        raise ValueError("oversized session answer")
    return notes, None, text


def interrupt(session):
    """Best-effort remote cancellation; local success is never claimed."""
    if session is None:
        return
    try:
        api("post", "/api/session/" + session + "/interrupt", {}, timeout=2.0)
    except (OSError, ValueError, SystemExit):
        pass


def terminate(signum, frame):
    """Unwind the active subprocess seam, then attempt remote interruption."""
    raise SystemExit(128 + signum)


def emit(payload):
    frame = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
    sys.stdout.buffer.write(frame.encode("utf-8") + b"\n")
    sys.stdout.buffer.flush()


def worker(model):
    """No terminal output except bounded newline JSON frames; fail closed."""
    signal.signal(signal.SIGTERM, terminate)
    session = None
    turn = None
    while True:
        if select.select([sys.stdin], [], [], 0.1)[0]:
            line = sys.stdin.buffer.readline(16384)
            if not line:
                return 0
            try:
                if not line.endswith(b"\n"):
                    raise ValueError("oversized wire request")
                request_frame = loads(line)
                if not isinstance(request_frame, dict):
                    raise ValueError("invalid wire request")
                fields = set(request_frame)
                if fields == {"prompt", "instructions"} and turn is None:
                    if session is None:
                        session = create(model)
                        inspect(session)
                    turn = begin(session, request_frame["prompt"], request_frame["instructions"])
                elif fields == {"decision"} and turn is not None and turn["awaiting"] is not None:
                    decide(session, turn["awaiting"], request_frame["decision"])
                    turn["deadline"] += time.monotonic() - turn["awaitingSince"]
                    turn["awaiting"] = None
                    turn["nextCheck"] = 0.0
                else:
                    raise ValueError("invalid wire request")
            except (ValueError, OSError, KeyError, TypeError, AttributeError):
                print('{"ok":false,"text":"OpenCode session failed"}', flush=True)
                return 1
        if turn is None:
            continue
        try:
            notes, ask, done = advance(session, turn)
            for line in notes:
                emit({"note": line})
            if ask is not None:
                turn["awaitingSince"] = time.monotonic()
                emit({"ask": {"id": ask["id"], "label": label(ask)}})
            if done is not None:
                emit({"ok": True, "text": done})
                turn = None
        except (ValueError, OSError) as error:
            # One classified line before the terminal poisons the wire: without it
            # a failed turn is indistinguishable from a dead server.
            emit({"note": "session error: " + clip(str(error), 120)})
            interrupt(session)
            print('{"ok":false,"text":"OpenCode session failed"}', flush=True)
            return 1


def launch(owo, model):
    """Run terminal and disposable adapter, preserving existing OpenCode login."""
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        raise ValueError("session TUI requires terminal stdin and stdout")
    request_read, request_write = os.pipe()
    response_read, response_write = os.pipe()
    worker_child = None
    terminal = None
    old_term = signal.signal(signal.SIGTERM, terminate)
    try:
        os.set_blocking(request_write, False)
        os.set_blocking(response_read, False)
        argv = [sys.executable, str(Path(__file__).resolve()), "--worker"]
        if model:
            argv += ["--model", model]
        worker_child = subprocess.Popen(argv, stdin=request_read, stdout=response_write,
                                        stderr=subprocess.DEVNULL, start_new_session=True)
        os.close(request_read)
        os.close(response_write)
        request_read = response_write = -1
        argv = [str(ROOT / ".build/tui"), "--bridge-read", str(response_read), "--bridge-write", str(request_write)]
        argv += ["--update-payload", str(ROOT / ".build/payload"),
                 "--update-checksum", str(ROOT / ".build/cli_view.sha256")]
        if owo:
            argv += ["--owo"]
        terminal = subprocess.Popen(argv, pass_fds=(response_read, request_write))
        # The launcher is not the terminal's SIGINT consumer; Ctrl-C reaches the
        # C entry too, which restores ncurses. Parent waits for that clean exit.
        previous = signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            return terminal.wait()
        finally:
            signal.signal(signal.SIGINT, previous)
    finally:
        if terminal is not None and terminal.poll() is None:
            terminal.terminate()
            try:
                terminal.wait(timeout=3)
            except subprocess.TimeoutExpired:
                terminal.kill()
                terminal.wait(timeout=3)
        if worker_child is not None:
            worker_child.terminate()
            try:
                worker_child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                worker_child.kill()
                worker_child.wait(timeout=3)
        for fd in (request_read, request_write, response_read, response_write):
            if fd >= 0:
                os.close(fd)
        signal.signal(signal.SIGTERM, old_term)


def main():
    """Selecting session mode is explicit network/remote-agent authorization."""
    parser = argparse.ArgumentParser(description="OpenCode-backed C23 Primeagen TUI")
    parser.add_argument("--engine", choices=("opencode",), default="opencode")
    parser.add_argument("--model", help="OpenCode provider/model, including your configured Go provider")
    parser.add_argument("--owo", action="store_true")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        return worker(args.model) if args.worker else launch(args.owo, args.model)
    except (OSError, ValueError):
        print("[vex] session host could not start; build TUI and configure OpenCode V2 first", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
