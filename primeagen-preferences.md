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
