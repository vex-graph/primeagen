# Primeagen >w<

## Current State

An early **personal R5 CLI application**, kept outside the ecosystem tree.
Python owns the conversational loop, consent and bounded child supervision;
a native C23 binding consumes Harness's `Message` and API Haven's actual request
builders. Func is invoked as an independent compile-time tool, never embedded.

Implemented: explicit OpenAI-compatible, Anthropic and loopback-local model
configuration; a strict JSON-text answer/tool loop; interactive approval for every
tool action; native C23 workspace-confined read/list/create/write/edit/delete,
explicitly approved unsandboxed Bash, configured search-program delegation;
real Func compilation and separately
approved artifact execution. That original `chat` mode uses our own agent loop.
Optional `session` mode now uses OpenCode V2 for **text-only generation**, with a
Primeagen-owned propose/gate/execute loop through the existing native tools and
API Haven's actual EngineProvider/Harness bridge callbacks. Its login/model configuration
(including Go when configured) stays with OpenCode; Primeagen reads no auth files.

This is an initial implementation, not production-ready. Offline owner evidence
lives in `tests/primeagen` in the workspace's independent tests checkout.
Direct-provider behavior, macOS 14 runtime, Windows and GUI integration are unproven.
The existing R4 message value is reused, not a claim that Harness's planned agent
objects or server are implemented.

The **C23 ncurses TUI** lives in `cli/` and `tui/`, with normalized mouse
events in `tui/event/`. It is an offline interface preview, not yet the native
agent replacement: disconnected start, wrapped transcript scrolling, fixed input,
clickable Clear/Quit and optional `personality/owo.c` presentation mode.
It now starts disconnected and exposes a local `/connect` action (also clickable)
that restores the terminal and launches the existing OpenCode session host on
`opencode-go/deepseek-v4.1-flash`. Ordinary disconnected text is not sent or
silently appended as a pretend answer. The frontend is C; this handoff still uses
the Python host and is not completion of the native-runtime migration.
Explicit `session` connects this terminal to a host-controlled local conversation.
OpenCode receives text-generation requests, never autonomous remote tool prompts.
Optional narration and complete escaped tool proposals appear between model calls;
history is retained locally for this run, not a saved OpenCode conversation.
Offline owners prove local gating with real disposable native Bash/file actions.
The initial opt-in live owner failed on HTTP 400 before a proposal; a separate
diagnostic completed proposal/rejection/answer. Live availability remains unproven.

Explicit `/update` now reloads native terminal rendering and local command code
through Hotcwap, preserving resident draft/history and session pipes. Build the
candidate with `sh cli/build.sh` in another terminal, then type `/update` in the
running app. New submissions pause; an active turn finishes before checksum/ABI
admission, candidate restore and commit or rejection, then submissions resume.
The displayed generation identifies the running code. This is a trusted local
developer release, not a network updater or full agent-runtime migration.

The terminal now has **three columns**: a tightly stacked `⌘`/`▦` text-symbol
rail with shared line borders, a clickable People/Servers list (then a selected
server's channels), and the right-side chat with fixed input/status. The existing
OpenCode entry returns to the real configured chat. Sola/WhateverName and
Ecosystem/#channel1..3 are **offline previews**, not registered models or real
server conversations; selecting them hides the live draft/history and cannot
submit it. Returning to OpenCode preserves that chat's draft/history/scroll.
Below 64 columns the rail/list collapse; text symbols fall back to ASCII in a
non-UTF locale. `--list-width` changes the middle-column default. Automated
headless cells prove geometry/selection, not appearance or actual mouse hardware.

Model replies now use a small **C terminal Markdown adapter**: bold, emphasis
(portable underline fallback), headings, yellow inline/fenced code, and colored
speaker labels. Markers are removed before wrapping; repeated prose blank lines
compact to one while fenced code spaces and blank lines stay literal. Terminal
control bytes remain escaped, not executable. Strict/sanitized numeric and
headless ncurses owners check visible bytes and actual cell attributes, not
interactive appearance. This is a flat subset, not CommonMark: nested inline
styles, links, tables, full Unicode grapheme shaping and syntax highlighting are not offered.

## Inspiration and name

Inspired by **PAIcom's personal computer-assistant idea**, and by the CLI-first
workflow of tools such as Pi, `agy` CLI (from Google's Antigravity), and OpenCode. This is an independent project, not a fork or an
endorsement. It intentionally explores an AI-powered assistant.

**Not affiliated with, endorsed by, or associated with PAIcom and its developer, ThePrimeagen, or Prime Intellect's Prime Agent.** The name
does not imply any connection to them or their projects/associations.

## Build and use

Requires Python 3.11+ and a C23-capable Clang. No dependencies are downloaded.
From this checkout:

```sh
python3 build.py
sh tools/build.sh
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
The terminal build additionally requires `HOTCWAP_SOURCE_DIR` (repository root,
defaulting to the sibling ecosystem checkout). It consumes Hotcwap's loader;
Primeagen remains standalone rather than Kernel-registered.
Missing dependencies fail; nothing is fetched or installed implicitly.

### Use your existing OpenCode / Go configuration

First configure OpenCode itself with `/connect` and select a model with `/models`.
Go is a provider subscription, not a transferable ChatGPT/Codex token; its key
remains in OpenCode's own connection. Primeagen does not load `.env` in this mode.
Requires an installed, configured **OpenCode V2**, using its official `opencode api`
command for local server discovery/authentication (no SDK download or token copying).

```sh
sh cli/build.sh
./primeagen session --engine opencode --owo
# Selected Go model (also used for explicitly authorized answer smoke tests):
./primeagen session --engine opencode --model opencode-go/deepseek-v4.1-flash --owo
# Optional: select an exact provider/model ID from your OpenCode model list:
./primeagen session --engine opencode --model PROVIDER/MODEL --owo
```

### Default: local execution gate

Enter may consume your OpenCode subscription/quota. The terminal now uses
`src/controlled_session.py`: text-only `/api/experimental/generate` returns one
strict JSON proposal; Primeagen validates it, pauses at the native Tools approval
callback, executes only when permitted, and returns the result to the model.
No remote `/prompt`, `/shell` or permission reply is used by this path.

**Inspection is automatic** for workspace read/list, except `.env` and `.env.*`
reads. Bash, create/write/edit/delete and Func build/run **ask first**.
The complete escaped action is displayed above the fixed **Yes / Always yes / No**
bar. No starts no tool child and changes no file; the denied result is retained
and the loop continues. Always yes remembers only that exact tool and arguments
for this run; a different command asks again. There is no durable blanket grant.
Read contents and tool results reach subsequent model prompts. Bash remains
unsandboxed. Network search is not configured in this terminal mode; it rejects
rather than silently borrowing OpenCode's remote tools. Read/list replace the
remote glob/grep vocabulary in this initial slice.

Human waits have no timeout; closing the terminal cancels the queued action.
Generation has a 300-second request budget; native children have a 60-second
budget and bounded pipes. Agent history defaults to 512 KiB and each turn to 16
steps, with explicit exhaustion. OS argv limits may reject smaller transcripts.
Optional `note` prose arrives between tool proposals, not token-by-token; no
separate reasoning stream or saved remote conversation is offered. `/update`
does not reload the Python gate—restart to adopt this change. `sh cli/build.sh`
also builds the native tools now.

### Legacy remote adapter (diagnostics only; not the terminal default)

The retained diagnostic adapter's Enter sends a turn and may consume your subscription/quota. A new dedicated session
is created only on the first Enter; never attach to the current coding session.
Every session requests the default policy and checks the echoed rules before each
turn: **inspection is automatic** (read/glob/grep), while **edits, shell, web
fetch/search, external directories and `.env` reads ask first** and everything else
is denied. Every API call pins `x-opencode-directory` to the caller's working
directory rather than the CLI's potentially different default Location. This
corrects routing but does **not** establish reliable approval enforcement: the
live consent owner still observed completed Bash with no pending request reaching
Primeagen. Do not rely on this experimental session mode to gate execution yet.
When a request
arrives, all admitted scanner resources are shown, followed by
`Do you accept this Bash script: …?  [y] Yes  [a] Always yes  [n] No`.
A fixed decision bar stays visible while you scroll to inspect the request;
the live chat/tail is selected automatically. Legacy [o]nce/[r]eject keys also work.
Draft editing/sending pauses until you decide. The local adapter forwards your decision and owns no
rule itself — OpenCode enforces it and owns any saved "always" approval, so a
configured deny always wins. Approvals cannot be granted by the model, and a
pending question never expires the model deadline; requests are polled for the
whole turn, so one raised mid-turn appears as soon as it happens. Trusted
server/configuration/plugins are required: echoed rules and fake tests do not
prove general live server policy enforcement or sandboxing. The opt-in real Go
shell-consent owner is currently failing; one earlier successful ask/reject was
not repeatable and is not acceptance evidence. Saved approval behavior is unproved.
Only one bounded request is
shown at a time.

**The turn is a stream, not one lump.** Sending shows your prompt, then the turn
unfolds as it happens:

```text
You:
refactor the parser and show me the diff

OpenCode:
lemme do it for you
read: src/parser.c (lines 40-92)
thinking: the helper duplicates the call site, so extract it
ah interesting, the call site is the odd one
edit: src/parser.c
  - old line removed
  + new line added
shell: git diff --stat
working...

answer:
Done — extracted the helper and updated the call site

```

The speaker label appears once per turn. Model prose arrives inline as it is
produced, `thinking:` lines show the reasoning parts, and each tool call prints
the moment its arguments are known (`read:`, `shell:`, `websearch:`, `glob:`/
`grep:`, `write:`/`create:`, and `edit:` with a plain uncolored `-`/`+` list).
The newest message's prose is held back as the turn's conclusion and printed
under `answer:`, so the flow ends where it should; a turn that already streamed
its conclusion simply ends. `working...` is a live placeholder at the bottom that
moves down as output arrives and disappears when the turn ends. All of it is
bounded display text — never commands, never re-executed here; syntax coloring
stays the language layer's job.

**Scrolling.** The transcript scrolls up/down with the mouse/trackpad and
**Up/Down** arrows (one row), with **End** to return to the tail, whatever the list
selection is; the footer names them. The terminal now decodes **SGR mouse reports**
directly, with X10 fallback, instead of passing wheel-down through Apple's old
ncurses mouse ABI. Wheel codes 64/65 are distinct from shift-click. A real PTY
owner sends both directions through the production terminal and checks that the
viewport returns to newer history; Ctrl-T disables capture for terminal selection.
Physical mouse/trackpad behavior and appearance still need user acceptance.

OwO instructions come from the same canonical C function, not a copied Python prompt.
Input remains available while a turn runs; Enter while busy preserves it. Final
answers arrive after completion; tool activity is reported as `read:`/`edit:`/
`shell:` lines rather than executed locally. Clear/Ctrl-L changes the display
only, not history. Ctrl-Q closes the local worker and attempts remote
interruption; interruption is best effort, so remote work may outlive exit.
Remote history remains on OpenCode. Per-turn bounds are 300
seconds of remote **silence** (a long multi-tool turn is expected — every streamed
tool line restarts the clock, and your approval wait is excluded), a 25-second
bounded read for the newest message page (a slow page is retried, not fatal), 16
KiB request framing, 64 KiB reply framing, a separate 8 MiB bound on that page
(tool arguments carry real file content) and the 200 newest remote messages.
When a turn does fail, the transcript prints one classified `session error: …`
line before closing — without it a failed turn is indistinguishable from a dead
server. A failed session is closed rather than reused: Enter explains that and
Ctrl-Q plus relaunch reconnects. Exhaustion still fails explicitly, never
silently truncates. Broader provider/subscription coverage, appearance, full
launcher PTY lifecycle and macOS 14 runtime evidence remain gaps.
**Codex App Server is not implemented yet.**

The user-authorized native-Haven/host answer smoke passed on
`opencode-go/deepseek-v4.1-flash` and included canonical OwO guidance. This narrow
arithmetic result is not general model competence, visual approval or remote-policy proof.

### Try the C23 terminal interface

Requires the host ncurses headers/library (no automatic installation):

```sh
sh cli/build.sh
./primeagen tui
./primeagen tui --owo
sh install-command.sh
primeagen
```

Running `./primeagen` with no arguments also opens the terminal. Enter `/connect`
(or click it in the lower hint line), then type your message and press Enter.
This uses your existing OpenCode Go connection for text generation and can consume
Go quota; it never attaches to this coding session. If Go is not
configured, connect **OpenCode Go** using OpenCode's own `/connect` first.
Primeagen does not accept/store an API key here. The handoff starts a fresh
terminal view; disconnected drafts are not transferred. `/help`, `/clear`
(display only), `/update`, and `/quit` are local commands; unsupported slash commands are
reported locally, never sent to the model. `/settings`, real server/channel session
routing, token-delta streaming and Zen key entry are not implemented. Tool approval
does exist: the default connected session uses the local execution gate above,
not the unreliable legacy remote permission queue.

Wheel-up/down and Up/Down scroll the transcript; End returns to the tail.
Click the input
line or press Tab to focus, or click Clear/Quit. Ctrl-L clears; Ctrl-Q quits. Enter
sends only after connection; disconnected input makes no model/tool call.
Mouse reporting depends on the terminal.
Click `⌘` for People, `▦` for Servers, then click a list entry. Ctrl-P/Ctrl-S
switch the middle panel; Ctrl-O returns to OpenCode. Only the right transcript
scrolls. Preview entries cannot edit or send the resident chat draft.
For highlighting/copying text, press **Ctrl-T**: mouse reporting turns off and
the actual chat opens full-width, so terminal-native drag selection does not
include the navigation columns. Drag to highlight, then use your terminal's Copy
command (for example Cmd-C on macOS). Ctrl-T returns to the three-column browser.
`primeagen tui --select-text` starts in that mode. Selection/clipboard behavior
belongs to your terminal; this is not an application-owned selection buffer or
automatic clipboard write. New output or scrolling may invalidate a selection.
Answers now travel as UTF-8 JSON from the host and render through wide ncurses
cells, including emoji, accented letters and CJK. UTF-16 surrogate pairs in
provider JSON are decoded by Python before the native reply is emitted; malformed
surrogates reject rather than emitting invalid UTF-8. Transcript wrapping counts
terminal cells, not bytes, and never splits a UTF-8 scalar. Invalid bytes, terminal
controls and bidi overrides remain escaped. Input editing is still printable
ASCII; full grapheme/ZWJ emoji shaping and font-width agreement remain gaps.
This fixes the Primeagen reply path, not the shared native JSON parser's separate
BMP-only escape-decoder limitation.

`sh install-command.sh` links this checkout's shell launcher into `~/.local/bin`
without overwriting an existing command or editing shell configuration. If that
directory is on PATH, `primeagen` works from any directory and preserves the
caller's working directory. An explicit alternative is
`sh install-command.sh /your/bin`. The link requires this checkout to stay in
place; this is not a standalone distributable install. Build explicitly first.
Input editing is currently printable ASCII; safe UTF-8 transcript scalars display
normally, while invalid/control bytes are escaped.
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

## Native tools and JSON tags

Build explicitly with `sh tools/build.sh`. Native actions live in
`tools/read.{c,h}`, `list`, `create`, `write`, `edit`, `delete`, `bash` and
`websearch`, with shared `fs` admission and a private `main.c` dispatcher.
The existing `primeagen chat --provider ... --model ...` agent requests one action
at a time using strict JSON, for example:

```json
{"tool":"read","arguments":{"path":"src/main.c"}}
{"tool":"list","arguments":{"path":"."}}
{"tool":"create","arguments":{"path":"new.txt","text":"new content"}}
{"tool":"write","arguments":{"path":"existing.txt","text":"replacement"}}
{"tool":"edit","arguments":{"path":"existing.txt","old":"unique text","new":"replacement"}}
{"tool":"delete","arguments":{"path":"file.txt"}}
{"tool":"bash","arguments":{"command":"git status --short"}}
{"tool":"websearch","arguments":{"query":"C23 documentation"}}
```

In original `chat` mode every action requires host/user approval, including read/list;
the default terminal's controlled session automatically permits non-secret read/list.
Unknown
tags, extra keys and model-provided approval fields reject. Python remains the
consent/provider/bounded-child host; actual file actions and exec delegation are C.
Denial spawns no child and mutates nothing. Results/errors return to the existing
agent loop; completed side effects are not undone when a later step fails.

File paths are relative to `--workspace`. Parents must exist; no traversal,
symlink or regular-file hardlink aliases. Create never overwrites, write replaces
an existing regular file, edit needs exactly one nonempty exact match, delete
removes one regular file without recursion or undo. Replacement changes inode
and preserves permission bits, not xattrs/ACLs/ownership metadata. Byte bounds
reject rather than silently truncate; `--tool-byte-cap` chooses the host budget.
Stable trusted tree required: this is not a racing-writer sandbox or crash-durable
transaction. Killing during staging can leave a private `.primeagen-*` file.

**Bash is unsandboxed**: approved commands inherit host environment/privileges,
can escape the workspace, access secrets, write/delete files and contact networks.
The host applies whole-child deadlines, bounded pipes and group cancellation/reap.
A refused group cleanup is reported and remains unresolved; it is not proof that
descendants stopped. Detached malicious processes remain outside the contract.

Websearch requires `--websearch-tool /absolute/path/to/trusted-search-program`.
That program receives the query as one literal argument and must use authorized
provider APIs. No default scraper, guessed provider or implicit installation is
provided; fake executable tests prove delegation only, not live search quality.

Registered mirrored `tests/primeagen/tools/*_test.py` owners compile/run strict C23
and ASan/UBSan tools; the shared fs C client and tagged-agent/consent owners use
disposable storage. Allocation/OS failure injection, full leak accounting, crash
recovery, hostile writers and other-platform proof remain gaps. The connected
legacy remote OpenCode session is also no longer deny-all: its default policy and
approval bridge remain diagnostic-only and unreliable. The default terminal now
uses these local C adapters behind the host-controlled execution gate instead.

## Scope and Limitations

- First model protocol is strict JSON **text**, not native structured tool calls;
  providers/models that do not follow it reject. Our own `chat` loop has no
  streaming, retries, native MCP, multi-agent routing or GUI. Only optional
  legacy diagnostic OpenCode adapter retains remote sessions. The default
  controlled terminal has local run-only history and between-step narration,
  no durable session storage, separate reasoning or token-delta streaming.
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
- The three-column browser is an initial presentation layer, not multi-model
  session routing, server membership, persistent channel histories or role/tool
  enforcement. Example entries are visibly labelled previews and make no calls.
- Updates reload only `cli/view.c` rendering/command exports. Host, Tui layouts,
   provider adapters, roles and tool integration require separate implementation/restart.
  SHA256 references come from the trusted local builder, not model output.
  Constructors/native modules are trusted, not sandboxed. Complete-set checks and
  detected rollback are tested; crash/power-loss recovery, signatures, hostile
  directories, fully ASan-instrumented dylib reload, leaks and other platforms
  remain gaps. An unresolved disk rollback stops the terminal, never resumes it.
