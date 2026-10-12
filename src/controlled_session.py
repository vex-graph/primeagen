"""Host-controlled terminal agent using OpenCode's text-only generation API.

============================================================================
CLASS: ControlledSession (src/controlled_session.py)
============================================================================
INSTANCE FIELDS (construction order):
  api;          borrowed official-CLI API callable; text generation only
  emit;         borrowed bounded native wire-frame writer
  receive;      borrowed human frame reader; never supplied by the model
  model;        selected provider/id or None for OpenCode's configured default
  always;       owned exact (tool, canonical arguments) approvals for this run
  instructions; current per-turn personality guidance, not permission
  tools;        owned existing native Tools dispatcher and child artifacts
  agent;        owned Agent with retained, bounded local history
FUNCTION REGISTRY:
  __init__(api, emit, receive, model, workspace, native) : validate model;
      compose existing Agent/Tools, without creating an autonomous remote session.
  complete(transcript) : one text-only generation; validate optional narration
      and return a strict answer/tool proposal to Agent, never execute remotely.
  approve(name, arguments) : automatic read/list except .env; otherwise show the
      complete escaped action, pause, validate one decision, and return permission.
  ask(prompt, instructions) : run serial propose/gate/execute/result continuations.
  close() : release tool artifacts after all child borrows finish.
MODULE FUNCTIONS:
  receiveFrame() : bounded newline JSON from the human terminal; EOF cancels.
  worker(api, emit, model) : one serial prompt loop, classified failure and close.
CONTRACT:
  OpenCode owns login/model generation only. /experimental/generate has no tools;
  Primeagen owns execution through Tools, whose validation precedes approval and
  whose child creation follows it. No remote prompt/shell/permission endpoint is
  called. No model output grants consent. Always applies to exact arguments for
  this process only; No retains a denied result and continues the model loop.
  Human wait has no deadline; SIGTERM/EOF cancels, without spawning the proposed
  action. Model and child work have separate bounded budgets. Read/tool contents
  reach subsequent model prompts. Trusted server/native executable/workspace,
  external file-writer exclusion and explicitly unsandboxed Bash remain required.
  Local history has Agent's byte/step bounds; no token/reasoning stream, persistent
  conversation, remote cancellation guarantee, network search configuration or
  sandbox claim. Owner-thread-only. The legacy remote session adapter is separate.
============================================================================
"""
import json
from pathlib import Path
import sys
import uuid

from agent import Agent, POLICY, strict_json
from json_input import loads
from tools import Tools

FRAME_CAP = 16384  # Same native request safety bound as SessionBridge.
NOTE_CAP = 398  # ASCII-escaped note bytes, below native note capacity.
GENERATION_TIMEOUT = 300.0  # One text-generation request, not human wait.


class ControlledSession:
    def __init__(self, api, emit, receive, model=None, workspace=None, native=None):
        if model is not None:
            provider, separator, identifier = model.partition("/")
            if not separator or not provider or not identifier or any(ord(c) < 33 for c in model):
                raise ValueError("model must be provider/model")
        self.api, self.emit, self.receive = api, emit, receive
        self.model = model
        self.always = set()
        self.instructions = ""
        self.tools = Tools(workspace or Path.cwd(), self.approve, native=native)
        self.agent = Agent(self, self.tools)

    def complete(self, transcript):
        policy = POLICY.replace(
            "Host approval is required for EVERY tool, including reads. You cannot grant consent.",
            "Read/list inspection is automatic except .env secrets. All other tools require host consent. "
            "You cannot grant consent.")
        policy += ('\nYou may add an optional "note" string beside an answer or tool proposal '
                   'to narrate your next step. Never claim execution before receiving its result.')
        prompt = self.instructions + policy + transcript[len(POLICY):]
        body = {"prompt": prompt}
        if self.model:
            provider, _, identifier = self.model.partition("/")
            body["model"] = {"providerID": provider, "id": identifier}
        result = self.api("post", "/api/experimental/generate", body,
                          timeout=GENERATION_TIMEOUT, cap=self.agent.history_cap)
        data = result.get("data")
        if not isinstance(data, dict) or not isinstance(data.get("text"), str):
            raise ValueError("invalid text-generation response")
        action = strict_json(data["text"])
        if not isinstance(action, dict):
            raise ValueError("model action must be an object")
        note = action.pop("note", None)
        if note is not None:
            if not isinstance(note, str) or len(note.encode("utf-8")) > FRAME_CAP:
                raise ValueError("invalid narration")
            escaped = json.dumps(note, ensure_ascii=True)[1:-1]
            for at in range(0, len(escaped), NOTE_CAP):
                self.emit({"note": escaped[at:at + NOTE_CAP]})
        return json.dumps(action, ensure_ascii=True)

    def approve(self, name, arguments):
        # Tools calls this only after exact schema/path/value admission and before
        # any native child is created. Approval is never parsed from model text.
        encoded = json.dumps(arguments, ensure_ascii=True, sort_keys=True, separators=(",", ":"))
        identity = (name, encoded)
        self.emit({"note": name + ":"})
        for at in range(0, len(encoded), NOTE_CAP):
            self.emit({"note": "  " + encoded[at:at + NOTE_CAP]})
        basename = Path(arguments.get("path", "")).name
        inspection = name == "list" or (name == "read" and basename != ".env" and not basename.startswith(".env."))
        if inspection or identity in self.always:
            return True
        request_id = "per_" + uuid.uuid4().hex
        label = "this Bash script" if name == "bash" else name + " action"
        self.emit({"ask": {"id": request_id, "label": label}})
        try:
            frame = self.receive()
        except (ValueError, OSError) as error:
            # Agent treats ordinary tool ValueErrors as results and continues.
            # Broken human control must instead terminate the worker.
            raise RuntimeError("invalid human decision frame") from error
        if not isinstance(frame, dict) or set(frame) != {"decision"} or frame["decision"] not in ("once", "always", "reject"):
            raise RuntimeError("invalid human decision")
        decision = frame["decision"]
        if decision == "always":
            self.always.add(identity)
        self.emit({"note": "denied by user" if decision == "reject" else "approved by user"})
        return decision != "reject"

    def ask(self, prompt, instructions):
        if not isinstance(instructions, str) or "\0" in instructions:
            raise ValueError("invalid instructions")
        self.instructions = instructions
        return self.agent.ask(prompt)

    def close(self):
        self.tools.close()


def receiveFrame():
    line = sys.stdin.buffer.readline(FRAME_CAP)
    if not line:
        raise EOFError("terminal closed")
    if not line.endswith(b"\n"):
        raise ValueError("oversized wire request")
    frame = loads(line)
    if not isinstance(frame, dict):
        raise ValueError("invalid wire request")
    return frame


def worker(api, emit, model):
    session = ControlledSession(api, emit, receiveFrame, model)
    try:
        while True:
            try:
                frame = receiveFrame()
            except EOFError:
                return 0
            if set(frame) != {"prompt", "instructions"}:
                raise ValueError("invalid wire request")
            answer = session.ask(frame["prompt"], frame["instructions"])
            if len(json.dumps({"ok": True, "text": answer}, ensure_ascii=False).encode("utf-8")) >= 65535:
                raise ValueError("oversized session answer")
            emit({"ok": True, "text": answer})
    except EOFError:
        return 0
    except (ValueError, OSError, KeyError, TypeError, AttributeError, RuntimeError) as error:
        # Do not expose model/tool contents through diagnostics on rejected input.
        emit({"note": "session error: " + type(error).__name__})
        emit({"ok": False, "text": "Controlled session failed"})
        return 1
    finally:
        session.close()
