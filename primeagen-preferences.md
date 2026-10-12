# Primeagen — Personal R5 Preferences

## Constitution Link

The complete [workspace constitution](https://gist.github.com/vex-graph/4132a6c45cb6d3797c3e8eff2e94035a)
governs this project. Workspace path: `../../gist/PREFERENCES.md`. Read it first,
then this document, `../../tests/test-preferences.md`, current
`../../gist/test-checklist.md`, and actual implementation.

## Law Index (Binding Matrix)

| Law Title | Scope | Enforcement |
| :--- | :--- | :--- |
| Personal Agent Boundary Law | Standalone R5 app and dependencies | No reverse includes, implicit fetches or duplicate ecosystem owners |
| Explicit Tool Consent Law | Read/build/run and model output | Offline denial, malformed-action and bounded child tests |

### Personal Agent Boundary Law

Primeagen lives outside the ecosystem tree and can be checked out anywhere. It
is a personal R5 consumer, not a replacement R4 Harness or R3 connector. Python
is the cold CLI/policy/host adapter; a C23 binding consumes real Harness Message
and API Haven request envelopes. Dependencies are supplied by source-root options;
Func is an independent bounded tool invocation, never an included implementation.
The standalone host supplies HTTPS transport for rendered requests because the
existing native send API has no TLS. This scoped consumer-adapter exception under
the Conflict Triage Law does not move ecosystem transport ownership or permit
reverse dependencies. No GUI/window/memory subsystem is implemented here.

The requested native direction starts with a C23 ncurses frontend in `cli/` over
the independently owned `tui/` model and `tui/event/` normalized terminal-cell
events. This offline preview does not replace the Python provider/agent loop yet.
The launcher defaults to this disconnected frontend. Its local `/connect` restores
the terminal before invoking the existing Python session host on the expressly
selected Go Flash model, reusing OpenCode login. No key entry/auth copying or
tool approvals are added. Unsupported slash commands stay local. This is a
transitional connected frontend, not a completed C runtime or server/channel runtime.
Personality `personality/owo.c` owns canonical AI speech-style instructions, never
consent or tool policy. Explicit chat --owo selects native prompt injection;
offline TUI greeting/status only reflect that mode. Prompt guidance is not model
compliance or vendor system-role proof. Vexspoke's actual JSON string writer is consumed, not duplicated.
Display buffers use cold growable C storage; no realtime/backend-trust claim.
The terminal navigation is a three-column rail/list/chat presentation. Connected
line-bordered text-symbol buttons select People or Servers; the middle panel
lists entries or a selected server's channels. Actual configured OpenCode chat
remains separate from explicit offline example people/channel previews. Previews
never send or display that chat's draft/history. New navigation is passed by
value to a new view export; the legacy draw ABI and resident Tui layout remain
unchanged. This does not implement model registries, channel routing or roles.
Ctrl-T/--select-text hands drag selection back to the terminal by disabling mouse
reporting and temporarily painting the actual chat full-width, without navigation
text mixed into multiline selections. Terminal owns highlighting/copy; no clipboard
mutation, application selection buffer or physical terminal proof is implied.
Host replies are strict UTF-8 JSON, not ASCII-escaped surrogate halves. Python
decodes provider JSON UTF-16 pairs before encoding native wire bytes; isolated
surrogates fail. Transcript admission preserves safe UTF-8 and escapes malformed
bytes/terminal controls/bidi overrides; row projection never splits a scalar.
Wide ncurses output measures wcwidth under stable host LC_CTYPE, not byte counts.
Input remains ASCII; full grapheme/ZWJ shaping and terminal font-width agreement
remain gaps. This does not amend the shared native JSON parser's BMP-only escapes.
The explicit install-command.sh links the real shell launcher into a per-user
bin directory, preserving unrelated commands, shell config and caller cwd.
Terminal Markdown is a Primeagen presentation adapter, not a guessed Vexspoke API.
Tui sanitizes controls before parsing and owns parallel visible-byte style flags;
ncurses maps only host-defined flags, never model ANSI. Formatting precedes wrap,
compact prose does not change remote messages, and fenced code whitespace remains
literal. The flat subset and emphasis fallback do not imply CommonMark/full Unicode shaping
or appearance acceptance. Styling owns no transport, credentials or permission.

The default `session --engine opencode` terminal now selects
`src/controlled_session.py`: OpenCode's official text-only
`/api/experimental/generate` supplies strict JSON proposals without remote tools.
Primeagen's existing Agent/Tools loop validates each action and stalls before
native child creation. Workspace read/list inspection is automatic except `.env`
and `.env.*`; every other available action asks. Yes approves once; Always yes
remembers exact tool/arguments for this process only; No retains denial and
continues. Complete escaped arguments precede the existing fixed approval bar.
Model content cannot supply human decisions. Malformed control is fatal; EOF
cancels queued work without execution. History is local/run-only, optional prose
arrives between steps, and generation/children have separate finite budgets.
No remote session prompt, shell or permission-reply endpoint is used by this
default. Search is unconfigured and rejects, not a remote-tool fallback.
This consumes the native SessionBridge/Harness callbacks without moving their
ownership. Existing unsandboxed Bash/stable-workspace/tool lifetime constraints
below still apply. Default local gate owner tests prove native disposable
execution and denial; initial live generation owner failed HTTP400 before a
proposal, while a separate diagnostic passed proposal/reject/continuation.
Availability, provider breadth and visual acceptance remain gaps.

The following remote adapter description is legacy diagnostic scope, not the
public terminal default. Its consent failure is retained, never promoted away.
The legacy adapter consumes API Haven's existing EngineProvider
and Harness_run/poll/cancel contracts through a native SessionBridge. The standalone
Python host invokes installed OpenCode V2's official API CLI as a bounded tool;
OpenCode owns authentication, provider/model selection and remote agent execution.
This narrow consumer-adapter exception does not duplicate an ecosystem connector
or move transport ownership. No auth-file/token copying, implicit installs or
existing-session attachment. New dedicated sessions request the default policy
(inspection automatic; edits, shell, web fetch/search, external directories and
`.env` reads wait for an explicit terminal decision; everything else denied),
verify the echoed rules before each turn and answer approvals only from the
terminal. The native adapter forwards the human's once/always/reject reply and
owns no rule itself; OpenCode enforces the decision and owns any saved project
approval, so a configured deny always wins. Server/config/plugin
trust and live policy enforcement remain explicit gaps. Personality instructions
are taken from canonical C Owo_instructions. Native TUI polls borrowed nonblocking
pipes, not sockets or workers. Derived SessionBridge wire buffers/identities change
only through admission/decide/poll/free (Conflict Triage Law managed accessor
exception).
A turn is presented as a stream, not one lump: model prose and `thinking:` reasoning
parts arrive inline as they are produced, each tool call prints the moment its
arguments are reported, and the newest assistant message's prose is held back as
the turn's conclusion under `answer:`. The session host emits these as bounded
`{note:...}` lines and the bridge surfaces exactly one frame per poll through
`SessionBridge_takeNote`, so a burst stays ordered and none is dropped. The
terminal appends them under one per-turn speaker label and keeps a live trailing
`working...` placeholder that is popped before any real output. Notes are bounded
display text derived from remote content (a `-`/`+` diff list for edits), never
commands and never a local re-execution; coloring stays the language layer's job.
Transcript scrolling is wheel-up/down, Up/Down one row and
End to tail, available whatever the list selection. The terminal owns bounded
SGR/X10 mouse decoding through ncurses prefix keys, never the v1 MEVENT fifth-slot
modifier alias. Capture is explicitly disabled for Ctrl-T and normal shutdown.
The production-process PTY owner proves both directions; physical mouse/trackpad
behavior and appearance remain user acceptance, not inferred from that owner.
Every official API invocation pins x-opencode-directory to the same caller cwd
Location as the created chat. Session policy echo alone did not prove Location
alignment or reliable approval enforcement: the live Go consent owner still
observed completed Bash without a pending request reaching this host. One prior
ask/reject success was not repeatable; the connected consent gate remains unresolved.
Approval selects the live chat/tail, shows all admitted scanner resources and a
fixed Yes/Always yes/No decision bar; scroll inspection remains available while
draft editing/sending pauses. Scanner resources are not full original shell source.
General tool enforcement and saved-always behavior remain unproved.
Timing is an idle watchdog, not a total budget. The Python host restarts its
window whenever the newest remote message changes and the bridge restarts its own
on every consumed frame, so a multi-tool turn is bounded by remote silence rather
than by how long the work takes; the human approval wait is excluded from both.
Pending approvals are polled for the whole turn, not only while the prompt message
is off the page, because the agent blocks on a request wherever it sits in the
turn. A slow newest-message read is retried rather than fatal (it has its own
bounded budget separate from the frame caps, because tool arguments carry real
file content), and a failed turn prints one classified `session error: ...` line
before the wire closes.
Cold projections redact transcript buffers and summarize the borrowed Haven driver,
whose current class has no bounded projection; never dump prompt/response values.
Remote history remains with OpenCode; local cancellation attempts remote interrupt
but does not guarantee it. Tool execution happens inside OpenCode under the
enforced policy: the local `tools/` C adapters are a separate `chat` path and are
not reachable from this session. It is not a native-agent replacement or sandbox.
Codex App Server remains unimplemented.

Chat may read checkout-root Git-ignored `.env` for OPENAI_API_KEY and
ANTHROPIC_API_KEY only. File parsing is bounded, literal and failure-preserving;
no execution/interpolation or content-bearing diagnostic. Existing environment
wins. Doctor/build/offline TUI do not load it. It remains local plaintext, never
uploaded by default, and is not a secure store or hostile-directory sandbox.

### Explicit Tool Consent Law

The terminal's explicit `/update` consumes Hotcwap `Hot_updateCold` and trusted
local build SHA256 references. Only view/command code is reloadable; terminal,
Tui/session pipes, approvals and remote permission configuration stay resident.
New prompts pause until an existing turn finishes and candidate admission resolves.
Failure retains running code/state; unresolved disk rollback stops the host.
Native candidates/constructors and stable parent directories are trusted, never
model-authorized code. No network download, broad native agent update, server/role
implementation, crash recovery or sandbox is implied. The named offline owner is
`tests/primeagen/cli/update_test.py`, with headless cells and scripted PTY evidence;
appearance and live remote session policy remain separate proof.

Model output is untrusted data, never authorization. Every tool needs separate
host/user approval. The exact JSON tool tags are read, list, write, edit, create,
delete, bash and websearch, plus the existing Func build/run tags. Their cold C23
file/exec adapters live in `tools/<name>.{c,h}`, with shared fs admission and a
private CLI entry. Python remains the consent/provider/whole-child-budget adapter.
This is the constitution's explicit standalone R5 consumer exception, not a new
ecosystem IO/NIO, process or network driver. No live provider/tool claim follows
from fake tagged-agent tests. The connected OpenCode session now runs its own
approval bridge under the default policy above; that remote policy is OpenCode's,
separate from these local C adapters, and both remain distinct from the
unimplemented server/channel/role runtime.

Denial never spawns a tool child or mutates files. File paths are canonical and
workspace-relative; parents must exist, symlink/hardlink/nonregular aliases reject.
Create admits an absent file (0600), write replaces an existing file via staged
rename, edit replaces exactly one nonempty match, delete unlinks one regular file
without recursion/undo. Replacement changes inode and may lose ACL/xattr/ownership
metadata; it preserves permission bits, not general metadata or crash durability.
Stable trusted tree/external serialization required; hostile concurrent writers
are outside contract. Cancellation after publication does not roll it back; SIGKILL
may leave a staging file. These remain partial consumer adapters, not trusted
production persistence backends with complete fault/leak/platform proof.

Bash is a separately approved /bin/bash --noprofile --norc -c action. It inherits
host environment/privileges and can escape the workspace, read secrets, mutate
files and contact networks; file confinement does not sandbox Bash. Search requires
a host-selected trusted executable receiving one literal query argument and
using authorized APIs; no scraper, implicit installation or guessed provider.
Both replace the supervised child through exec, never system() or unbounded wait.
The host bounds output/time and attempts group cancellation/reap; group signalling
refusal is observable and remains an unresolved descendant-cleanup gap.

Build is not run permission. Func run still requires a session-created artifact;
no model-selected search executable or auto-approve surface is offered. Stable trusted
workspace and compiler inputs are required; path checks are not a sandbox.
Child execution has an overall deadline, simultaneous bounded output and kill/reap
cleanup. Each retained result reports failure or truncation explicitly. History
has a caller-selected safety budget; growth is not secretly capped by an initial
collection capacity. No concurrency or persistent session identity is claimed.

## Proof

Mirrored offline owners live in `../../tests/primeagen`. Test output/readiness
must distinguish real native envelope and Func artifact execution from fake model
responses. Live providers, OS/platform gaps and GUI approval remain separate.
Live answer smoke is opt-in only and uses the user's selected
`opencode-go/deepseek-v4.1-flash`, never a fallback paid model. A passed arithmetic
answer proves that narrow route, not general competence or remote sandbox policy.
