# Primeagen — Personal R5 Preferences

## Constitution Link

The complete [workspace constitution](https://gist.github.com/vex-graph/4132a6c45cb6d3797c3e8eff2e94035a)
governs this project. Workspace path: `../../preferences.md`. Read it first,
then this document, test laws/current checklist, and actual implementation.

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
The explicit install-command.sh links the real shell launcher into a per-user
bin directory, preserving unrelated commands, shell config and caller cwd.
Terminal Markdown is a Primeagen presentation adapter, not a guessed Vexspoke API.
Tui sanitizes controls before parsing and owns parallel visible-byte style flags;
ncurses maps only host-defined flags, never model ANSI. Formatting precedes wrap,
compact prose does not change remote messages, and fenced code whitespace remains
literal. The flat subset and emphasis fallback do not imply CommonMark/Unicode
or appearance acceptance. Styling owns no transport, credentials or permission.

Explicit `session --engine opencode` consumes API Haven's existing EngineProvider
and Harness_run/poll/cancel contracts through a native SessionBridge. The standalone
Python host invokes installed OpenCode V2's official API CLI as a bounded tool;
OpenCode owns authentication, provider/model selection and remote agent execution.
This narrow consumer-adapter exception does not duplicate an ecosystem connector
or move transport ownership. No auth-file/token copying, implicit installs or
existing-session attachment. New dedicated sessions request deny-all permissions,
verify the echo before each prompt and never send approvals. Server/config/plugin
trust and live policy enforcement remain explicit gaps. Personality instructions
are taken from canonical C Owo_instructions. Native TUI polls borrowed nonblocking
pipes, not sockets or workers. Derived SessionBridge wire buffers/identities change
only through admission/poll/free (Conflict Triage Law managed accessor exception).
Cold projections redact transcript buffers and summarize the borrowed Haven driver,
whose current class has no bounded projection; never dump prompt/response values.
Remote history remains with OpenCode; local cancellation attempts remote interrupt
but does not guarantee it. Initial session mode is chat-only, not tool consent or
native-agent replacement. Codex App Server remains unimplemented.

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

Model output is untrusted data, never authorization. Every read, Func build and
artifact run needs separate host/user approval. Denial performs no operation.
Build is not run permission. Only session-created artifact identities can run;
no arbitrary shell/executable or auto-approve surface is offered. Stable trusted
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
