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
Personality `personality/owo.c` owns canonical AI speech-style instructions, never
consent or tool policy. Explicit chat --owo selects native prompt injection;
offline TUI greeting/status only reflect that mode. Prompt guidance is not model
compliance or vendor system-role proof. Vexspoke's actual JSON string writer is consumed, not duplicated.
Display buffers use cold growable C storage; no realtime/backend-trust claim.

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
