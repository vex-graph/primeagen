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

The new **C23 ncurses TUI** lives in `cli/` and `tui/`, with normalized mouse
events in `tui/event/`. It is an offline interface preview, not yet the native
agent replacement: local text entry, wrapped transcript scrolling, fixed input,
clickable Clear/Quit and optional `personality/owo.c` presentation mode.

## Inspiration and name

Inspired by **PAIcom's personal computer-assistant idea**, and by the CLI-first
workflow of tools such as Pi. This is an independent project, not a fork or an
endorsement. It intentionally explores an AI-powered assistant.

**Not affiliated with, endorsed by, or associated with ThePrimeagen, or Prime Intellect's Prime Agent.** The name
does not imply any connection to them or their projects/associations.

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
them in argv, source or history. Chat also loads this checkout's Git-ignored `.env`
automatically (not the current working directory). Existing shell environment
variables take precedence, even if empty. Build, doctor and offline TUI do not
load the file. Put your key into the corresponding blank entry:

```dotenv
OPENAI_API_KEY=
ANTHROPIC_API_KEY=
```

Only these two names are accepted. Optional matching quotes are supported; no
shell commands, substitutions, multiline values or inline comments are executed
or interpreted. Invalid files reject before applying any new keys. The file is
plain text, not encrypted; keep it private with `chmod 600 .env`. Never approve
a tool read of `.env` or other secrets—approved read contents can reach the model.
Chat explicitly contacts the configured provider
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

### Try the C23 terminal interface

Requires the host ncurses headers/library (no automatic installation):

```sh
sh cli/build.sh
./primeagen tui
./primeagen tui --owo
```

Wheel/PgUp/PgDn scroll the transcript; End returns to the tail. Click the input
line or press Tab to focus, or click Clear/Quit. Ctrl-L clears; Ctrl-Q quits. Enter appends
locally and makes no model/tool call. Mouse reporting depends on the terminal.
Input editing is currently printable ASCII; other transcript bytes are escaped.
Automated headless ncurses cell assertions are not appearance or real-terminal
mouse acceptance. Normal exit and SIGINT/SIGTERM restore the terminal; crashes
and SIGKILL cannot promise restoration. Windows/Linux builds remain unproved.
`personality/owo.c` owns AI prompt instructions: playful human-facing prose,
without changing task completion, code, JSON/tool arguments or consent. Enable
them in the current agent with `./primeagen chat --provider openai --model YOUR_MODEL --owo`
after `python3 build.py`. The native request adapter prefixes these instructions
to message content; vendor system-role priority and live model compliance are
not proven. The offline TUI also identifies the selected mode in greeting/status,
but does not contact a model.

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
