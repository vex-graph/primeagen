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
  advance(session, turn) : one bounded progress step; reports ask/done/running.
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
  The whole-turn deadline excludes human decision time; bounded response/page/prompt
  safety caps, quiet generic failure. Remote interrupt is best effort; server work
  may outlive local timeout. Poll cadence is idle IO scheduling, not test
  synchronization or frame cadence. POSIX/macOS only; no native-agent, stream-delta
  or sandbox claim.
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
TURN_TIMEOUT = 120.0
WIRE_CAP = 65536
PAGE_LIMIT = 100  # Reject a single turn exceeding this bounded newest page.
REQUEST_LIMIT = 8  # Reject an implausible pending-approval page.
LABEL_CAP = 200  # Bounded native display text; the C bridge rejects overflow.
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


def api(method, path, body=None, timeout=10.0, empty=False):
    """Use official CLI discovery/auth, never inspect its token store."""
    argv = ["opencode", "api", method, path]
    if body is not None:
        argv += ["--data", json.dumps(body, ensure_ascii=True, separators=(",", ":"))]
    raw = run(argv, timeout=timeout, cap=WIRE_CAP)
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
            "awaiting": None, "awaitingSince": 0.0, "nextCheck": 0.0}


def collect(item, answers, tools):
    """Project one assistant message; tool activity is reported, never executed here."""
    if not isinstance(item, dict) or item.get("type") != "assistant":
        return
    if item.get("error") or item.get("finish") in ("error", "length", "content-filter"):
        raise ValueError("agent turn rejected")
    content = item.get("content", [])
    if not isinstance(content, list) or len(content) > PAGE_LIMIT:
        raise ValueError("invalid assistant content")
    for part in content:
        if not isinstance(part, dict):
            raise ValueError("invalid assistant content part")
        kind = part.get("type")
        if kind == "tool":
            name = part.get("name")
            if not isinstance(name, str) or not name or len(name) > 64:
                raise ValueError("invalid tool identity")
            tools.append(name)
        elif kind == "text":
            text = part.get("text")
            if not isinstance(text, str):
                raise ValueError("invalid answer text")
            answers.append(text)


def advance(session, turn):
    """One bounded progress step: ask, running or done. Never blocks on a human."""
    if turn["awaiting"] is not None:
        return "waiting", None
    if time.monotonic() >= turn["deadline"]:
        raise ValueError("remote turn deadline exceeded")
    result = api("get", "/api/session/" + session + "/message?order=desc&limit=" + str(PAGE_LIMIT),
                 timeout=min(10.0, TURN_TIMEOUT))
    messages = result.get("data")
    if not isinstance(messages, list) or len(messages) > PAGE_LIMIT:
        raise ValueError("invalid session messages")
    boundary = next((i for i, item in enumerate(messages) if item.get("id") == turn["messageId"]), None)
    if boundary is None:
        if len(messages) == PAGE_LIMIT:
            raise ValueError("turn exceeded response page safety limit")
        if time.monotonic() >= turn["nextCheck"]:
            turn["nextCheck"] = time.monotonic() + PERMISSION_POLL
            pending = request(session)
            if pending is not None:
                turn["awaiting"] = pending
                return "ask", pending
        return "running", None
    answers, tools, completed = [], [], False
    for item in list(reversed(messages[:boundary])):
        collect(item, answers, tools)
        if isinstance(item, dict) and item.get("type") == "idle":
            if item.get("outcome") != "succeeded":
                raise ValueError("remote session did not succeed")
            completed = True
    if not completed:
        return "running", None
    text = "\n".join(answers)
    if tools:
        note = "used " + ", ".join(sorted(set(tools)))
        text = text + ("\n" if text else "") + note
    if not text:
        text = "(no text returned)"  # a completed turn may be tool-only or silent
    if len(json.dumps({"ok": True, "text": text}, ensure_ascii=False).encode("utf-8")) >= WIRE_CAP - 1:
        raise ValueError("oversized session answer")
    return "done", text


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
            state, detail = advance(session, turn)
            if state == "ask":
                turn["awaitingSince"] = time.monotonic()
                emit({"ask": {"id": detail["id"], "label": label(detail)}})
            elif state == "done":
                emit({"ok": True, "text": detail})
                turn = None
        except (ValueError, OSError):
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
