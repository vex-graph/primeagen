# Primeagen

## Current State

An early **personal R5 CLI application**, kept outside the ecosystem tree.
Python owns the conversational loop, consent and bounded child supervision;
a native C23 binding consumes Harness's `Message` and API Haven's actual request
builders. Func is invoked as an independent compile-time tool, never embedded.

Implemented: explicit OpenAI-compatible, Anthropic and loopback-local model
configuration; a strict JSON-text answer/tool loop; interactive approval for every
read/build/run; workspace-confined reads; real Func compilation and separately
approved artifact execution. No other coding-agent CLI is used as our agent.

This is an initial implementation, not production-ready. Offline owner evidence
lives in `tests/primeagen` in the workspace's independent tests checkout. Live
provider behavior, macOS 14 runtime, Windows and GUI integration are unproven.
The existing R4 message value is reused, not a claim that Harness's planned agent
objects or server are implemented.

## Inspiration and name

Inspired by **PAIcom's personal computer-assistant idea**, and by the CLI-first
workflow of tools such as Pi. This is an independent project, not a fork or an
endorsement. It intentionally explores an AI-powered assistant.

**Not affiliated with, endorsed by, or associated with ThePrimeagen.** The name
does not imply any connection to him or his projects.

## Build and use

Requires Python 3.11+ and a C23-capable Clang. No dependencies are downloaded.
From this checkout:

```sh
python3 build.py
./primeagen doctor
./primeagen chat --provider openai --model YOUR_MODEL
./primeagen chat --provider anthropic --model YOUR_MODEL
./primeagen chat --provider local --model YOUR_MODEL --base-url http://127.0.0.1:11434/v1
```

Hosted credentials come from `OPENAI_API_KEY` or `ANTHROPIC_API_KEY`; do not put
them in argv, source or history. Chat explicitly contacts the configured provider
and may incur charges. Build/doctor do not contact it. Read contents/tool results
become part of later prompts: approve only data you intend to send to that backend.
There is no unattended `--yes` flag. `/new` clears conversation; `/quit` exits.

Workspace discovery defaults use sibling Func and local ecosystem source
checkouts. A standalone checkout can live anywhere: set `API_HAVEN_SOURCE_DIR`,
`HARNESS_SOURCE_DIR`, `VEXSPOKE_SOURCE_DIR` to their `src` directories before
building, and set `FUNC_PATH` or `--func` to the Func launcher. These are explicit
source/tool dependencies, not a requirement to register Primeagen inside Hotcwap.
Missing dependencies fail; nothing is fetched or installed implicitly.

## Scope and Limitations

- First model protocol is strict JSON **text**, not native structured tool calls;
  providers/models that do not follow it reject. No streaming, retries, native
  MCP, persistent sessions, editing tools, multi-agent routing or GUI yet.
- Local means an OpenAI-compatible loopback server, not an embedded model or
  native Ollama protocol. Hosted transport verifies HTTPS and refuses redirects.
- The Python host transport is an explicit standalone consumer adapter. API Haven
  owns request envelopes; its unimplemented HTTPS send path is not invoked.
- Stable trusted workspace/parents required. Path checks are not a hostile-tree
  sandbox. External compilers and compiled artifacts are trusted code; process
  deadlines are not a sandbox against detached descendants.
- Func presently compiles literal add/mul/print only. General orchestration,
  references/control flow, arbitrary external commands and registry extensibility
  remain future work. Building is never implicit permission to execute.
- All work is cold-path; no zero-allocation/realtime or battle-tested claim.
