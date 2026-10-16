# Primeagen — the real thing (plan)

> Planning document. Nothing here is implemented; it records the agreed direction
> and the order of work so each slice can be built and proved in turn. Governed by
> the workspace constitution (`../../../PREFERENCES.md`), `primeagen-preferences.md`,
> `../../drivers/darkbase-engine/darkbase-preferences.md` and `tests/test-preferences.md`.

## The goal

Primeagen stops being a preview of an agent client and becomes the actual thing:
**your own users, your own servers, your own channels, your own messages, your own
models — over your own database and your own API keys.** No borrowed agent host,
no headless connection standing in for the product. Freedom means you hold the
identity, the storage and the credentials.

## The shape (what the words mean)

| Word | Means | Owned by |
| :--- | :--- | :--- |
| **User** | a local identity/persona: display name, handle, avatar, optional per-user defaults | Primeagen (R5) |
| **Server** | a *connection you own*: endpoint + provider family + credential reference + the models it offers. Not "someone else's service" — your row, your key | Primeagen (R5), credential via existing `.env`/keychain seam |
| **Channel** | a conversation on a server: title, model selection, its transcript | Primeagen (R5) |
| **Message** | one immutable row: author (user or model), body, timestamp, plus the tool/consent trail that produced it | Primeagen (R5) |
| **Store** | the native vex entity store that holds all of the above | **darkbase (R3)** |

## Ownership and the seam (Vertical Integration Law)

```
R5  personal/primeagen      users, servers, channels, messages, the window/form,
                            provider policy, the input line
        │ borrows an opaque Database* + fn-table (never an #include of R5)
R3  drivers/darkbase        the Database interface + native vex store (darkbase-db)
        │ borrows
R2  relational-engine / vexspoke   memory, files, native C search, CPU contracts
```

Primeagen never writes bytes into a file format itself and never grows a rival
query engine; darkbase never learns what a "channel" is. R3 exposes rows and
reflection; R5 decides meaning.

## Slices, in dependency order

Each slice ships with its owner test, `-Wall -Wextra -Werror`, and ready evidence
before the next begins. No slice is "started" to have something moving.

### S0 — the live frame (unblocks everything visual)

Wire `cli/screen.c` into the live app: `Primeagen_drawPanels`/`Primeagen_draw`
become `bind(getmaxyx) → renderHome/renderChat → present`, with a HOME screen
state so the landing menu is the start screen. Retire the direct-ncurses view and
the old-layout assertions in `view_test.c`/`main_test.c`.
*Definition of done:* `./primeagen` shows the frame; resize/zoom revalidates; the
suite is green on the new layout.

### S1 — the bottom text input

Wire the toolkit's `TerminalInputLine` (`bind`/`add`/`backspace`/`clear`/
`setPrefix`/`setPens`) into the frame so every screen has a real bottom input row
instead of a bare `> ` echo: caret, backspace, submit, prefix, and a bounded
buffer with an observable refusal when full.
*DoD:* typing reaches the app as a submitted line; owner test covers empty,
1-char, capacity, over-capacity, backspace-to-empty and clear.

### S2 — darkbase M1: the store actually stores

Build the R3 store behind the existing `Database` interface, in darkbase's own
law order:
1. **Entity model** — consume R2 reflection (`Struct`/`Field`/`Method`); no
   darkbase-owned schema types.
2. **Byte-native persistence** — `uint8_t[]` rows written in memory order, no
   endianness code, no text encoding, pointer-free rows, the canonical
   `{ uint8_t name[24]; void* pointer }` binding.
3. **Zero-alloc scans** — iterators yield pointers into the mapped row slices;
   no row wrappers in the loop.
4. **B-tree page consistency** — fixed power-of-two pages from a dedicated pool,
   atomic splits before parent updates.
5. **Reactive programs** — named store-event programs on the cold admission path
   only, rebinding by name on load.
6. **Store vs executable separation** — `darkbase-db` stays a resident library;
   `darkbase` (the workbench) is a separate future application.
*DoD:* create/open/insert/scan/commit/reopen round-trip proof, exhaustion and
recovery proof (the Deliberate Exhaustion and Backend Trust Law), torn-write
injection, and a cursor-boundary owner test. This is the long pole.

### S3 — Primeagen's entities over darkbase

Define User/Server/Channel/Message as reflection entities (C structs + `Field`
layout), one class pair each, and a thin Primeagen-owned store adapter that
borrows darkbase's opaque handle + fn-table. Local CRUD: create a user, create a
server (endpoint + provider + credential reference), create a channel, append and
read messages.
*DoD:* the four entities round-trip through the store; identity and ordering are
stable across reopen; no Primeagen code touches the file format.

### S4 — the direct provider path, made the default

Promote the existing native API-key path (`src/env_file.c` →
`OPENAI_API_KEY`/`ANTHROPIC_API_KEY`, plus loopback-local) to the real default:
a provider registry addressed by the Server rows, streaming responses into
Channels, and the consent-gated tool loop over the same messages that get
persisted. OpenCode session mode stays available but demoted to an optional
diagnostic, never the product path.
*DoD:* a real answer through a real key from a real Server row, streamed into a
Channel and persisted as Messages; failures classified and observable; offline
fake-provider owners for CI.

### S5 — live database

Point the app at a real `.vexdb` under Primeagen's own storage location; boot,
open, migrate-if-needed (with validation and rollback), and teardown in the
constitution's reverse order.

## Decisions still open (settle before the slice that needs them)

| # | Decision | Needed by |
| :--- | :--- | :--- |
| D1 | Credential storage: checkout `.env` only, or the OS keychain (`Keychain` seam) as the durable home? | S4 |
| D2 | Is a Server one endpoint+key, or a provider with several endpoints/models? | S3 |
| D3 | Are Messages append-only forever, or is there an edit/delete contract (and therefore a revision column)? | S3 |
| D4 | Multi-user locally (switch identities) or one local user per install? | S3 |
| D5 | Does Primeagen ever talk to a *remote* peer server (two installs syncing), or only to model endpoints? This decides whether identity has to be globally unique. | S3/S4 |
| D6 | Reaction programs in the store: which events actually need them (index maintenance? derived channel views?) | S2 |
| D7 | Confirm darkbase's first `.vexdb` format version and that no schema migration is needed before v1. | S2/S5 |

## Non-goals for now

- No sandbox claims: the file adapters remain non-sandboxed consumer adapters.
- No remote session/permission endpoints; no headless agent host as the product.
- No GUI; the ncurses frame stays the surface.
- No SQL. The native query contract is structured; text is at most a `cli/`
  convenience later.
