"""Optional OpenCode V2 session host for the native Primeagen terminal.

============================================================================
MODULE: Primeagen OpenCode host (src/session_host.py)
============================================================================
OWNED STATE:
  Launcher owns two pipes and terminal/worker children. Worker retains one newly
  created session ID in a local variable; OpenCode owns login/config/model state,
  history and remote execution. No credential files or .env are read here.
FUNCTION REGISTRY:
  api(method, path, body, timeout) : bounded literal OpenCode CLI API invocation;
      reject diagnostics, malformed/oversized/deep/duplicate JSON; never echo it.
  checked_id(value) : validate remote ID before embedding in literal API path.
  turn(session, prompt, instructions, model) : create isolated deny-all session,
      verify permission echo, admit prompt, poll newest messages through idle;
      reject tools/errors/pagination exhaustion, interrupt on exceptional paths.
  worker(model) : newline request/reply loop; final answers only, generic failures.
  terminate(signum, frame) : raise cancellation so bounded child cleanup executes.
  launch(owo, model) : pass nonblocking wire fds to native TUI; terminal owns display;
      on every exit terminate/kill/reap worker, close pipes. No implicit build.
  main() : explicit --engine opencode, optional provider/model, --owo or private
      --worker entry. Nonterminal launch rejects before starting any worker.
CONTRACT:
  Trusted installed OpenCode V2 executable/server; subscriptions remain with its
  login. Model is explicit provider/id when selected, otherwise OpenCode default.
  Never attaches to an existing session or this harness conversation. Each new
  dedicated session has deny-all tool permissions, no permission approvals sent.
  Session history stays on OpenCode server after exit; not ephemeral/private RAM.
  Successful remote admission can incur subscription use/charges. No token copying,
  automatic installs, project config changes, direct vendor API or Codex support.
  120s whole-turn deadline, bounded response/page/prompt safety caps, quiet generic
  failure. Remote interrupt is best effort; server work may outlive local timeout.
  Poll cadence is idle IO scheduling, not test synchronization or frame cadence.
  POSIX/macOS only; no native-agent, stream-delta, agent-tool or sandbox claim.
============================================================================
"""
import argparse
import json
import os
from pathlib import Path
import re
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
DENY_ALL = [{"action": "*", "resource": "*", "effect": "deny"}]


def api(method, path, body=None, timeout=10.0):
    """Use official CLI discovery/auth, never inspect its token store."""
    argv = ["opencode", "api", method, path]
    if body is not None:
        argv += ["--data", json.dumps(body, ensure_ascii=True, separators=(",", ":"))]
    result = loads(run(argv, timeout=timeout, cap=WIRE_CAP))
    if not isinstance(result, dict):
        raise ValueError("invalid API response object")
    return result


def checked_id(value):
    """IDs are not URLs, traversal, command fragments or borrowed session names."""
    if not isinstance(value, str) or not re.fullmatch(r"ses[A-Za-z0-9_]{1,128}", value):
        raise ValueError("invalid remote session identity")
    return value


def turn(session, prompt, instructions, model):
    """Retain one dedicated remote conversation; complete only on matching idle."""
    if not isinstance(prompt, str) or not prompt or len(prompt.encode()) > 8192:
        raise ValueError("invalid session prompt")
    if not isinstance(instructions, str) or len(instructions.encode()) > 4096:
        raise ValueError("invalid session guidance")
    deadline = time.monotonic() + TURN_TIMEOUT
    if session is None:
        body = {"title": "Primeagen chat", "location": {"directory": str(Path.cwd())}, "permissions": DENY_ALL}
        if model:
            provider, separator, identifier = model.partition("/")
            if not separator or not provider or not identifier or any(ord(c) < 33 for c in model):
                raise ValueError("model must be provider/model")
            body["model"] = {"providerID": provider, "id": identifier}
        result = api("post", "/api/session", body)
        info = result.get("data", {})
        session = checked_id(info.get("id"))
        if info.get("permissions") != DENY_ALL:
            raise ValueError("server did not confirm deny-all session permissions")
    path = "/api/session/" + checked_id(session)
    try:
        info = api("get", path).get("data", {})
        if info.get("permissions") != DENY_ALL:
            raise ValueError("session permissions changed")
        # Unique message ID gives an exact boundary: old idle/answers never satisfy
        # this turn. Model output is displayed, never interpreted as instructions.
        message_id = "msg_" + uuid.uuid4().hex
        api("post", path + "/prompt", {"id": message_id, "text": instructions + prompt})
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            result = api("get", path + "/message?order=desc&limit=" + str(PAGE_LIMIT), timeout=min(10.0, remaining))
            messages = result.get("data")
            if not isinstance(messages, list) or len(messages) > PAGE_LIMIT:
                raise ValueError("invalid session messages")
            boundary = next((i for i, item in enumerate(messages) if item.get("id") == message_id), None)
            if boundary is None:
                if len(messages) == PAGE_LIMIT:
                    raise ValueError("turn exceeded response page safety limit")
                time.sleep(0.1)
                continue
            current = list(reversed(messages[:boundary]))
            answers = []
            completed = False
            for item in current:
                if item.get("type") == "assistant":
                    if item.get("error") or item.get("finish") in ("error", "length", "content-filter"):
                        raise ValueError("agent turn rejected")
                    for content in item.get("content", []):
                        if content.get("type") == "tool":
                            raise ValueError("tool use is unsupported in chat-only session")
                        if content.get("type") == "text":
                            text = content.get("text")
                            if not isinstance(text, str):
                                raise ValueError("invalid answer text")
                            answers.append(text)
                if item.get("type") == "idle":
                    if item.get("outcome") != "succeeded":
                        raise ValueError("remote session did not succeed")
                    completed = True
            if completed:
                text = "\n".join(answers)
                if not text or len(json.dumps({"ok": True, "text": text}, ensure_ascii=True).encode()) >= WIRE_CAP - 1:
                    raise ValueError("empty or oversized session answer")
                return session, text
            time.sleep(0.1)
        raise ValueError("remote turn deadline exceeded")
    except BaseException:
        try:
            api("post", path + "/interrupt", {}, timeout=2.0)
        except (OSError, ValueError, SystemExit):
            pass
        raise


def terminate(signum, frame):
    """Unwind the active subprocess seam, then attempt remote interruption."""
    raise SystemExit(128 + signum)


def worker(model):
    """No terminal output except bounded newline JSON frames; fail closed."""
    signal.signal(signal.SIGTERM, terminate)
    session = None
    while True:
        line = sys.stdin.buffer.readline(16384)
        if not line:
            return 0
        try:
            if not line.endswith(b"\n"):
                raise ValueError("oversized wire request")
            request = loads(line)
            if not isinstance(request, dict) or set(request) != {"prompt", "instructions"}:
                raise ValueError("invalid wire request")
            session, text = turn(session, request["prompt"], request["instructions"], model)
            print(json.dumps({"ok": True, "text": text}, ensure_ascii=True, separators=(",", ":")), flush=True)
        except (ValueError, OSError, KeyError, TypeError, AttributeError):
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
    parser = argparse.ArgumentParser(description="OpenCode-backed C23 Primeagen TUI (chat-only)")
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
