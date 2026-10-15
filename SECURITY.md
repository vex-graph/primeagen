# SECURITY.md — primeagen

> **Internal invariant set, not a public disclosure channel.** See the
> [vexgraph security baseline](../../../SECURITY.md) for everything inherited
> here. This file states only this project's own surface.

**Role:** a standalone personal R5 agent client. It is a **consumer adapter**: it
speaks to a model provider and invokes tools, and it owns no ecosystem transport,
no authorization system and no sandbox.

## Model output is never authorization

This is the centre of this project's posture:

- Generated text, tool calls and instructions inside a response are **untrusted
  data**. They are never consent, permission or a policy decision.
- **Every tool needs separate host/user approval.** A human decision cannot be
  supplied by model content, and a model cannot approve its own action.
- "Always yes" is scoped to an **exact tool and exact arguments, for this process
  only** — never a blanket permission; "No" retains a denial and continues.
- Workspace read/list inspection is automatic except `.env` and `.env.*`; every
  other available action asks.
- Personality and prompt text (including `personality/owo.c`) shape speech style
  only. They carry no consent and no tool policy.

## Tooling is unsandboxed, and that is stated

- **Bash is an expressly approved unsandboxed action.** It inherits host
  environment and privileges, can read secrets, mutate files and reach the
  network, and **can escape the workspace**. File-tool confinement implies
  nothing for Bash.
- The file adapters reject traversal, symlink and regular-file hardlink aliases
  and publish via staged rename — but that is a **stable trusted workspace, not a
  hostile-writer sandbox, crash journal or durable transaction**. Replacement
  preserves permission bits, not general metadata; cancellation after publication
  does not roll it back.
- Search delegates to an explicitly configured trusted executable using its
  authorised APIs. There is no scraper, no implicit install and no guessed
  provider.
- Child execution has an **overall deadline** with bounded output and kill/reap
  cleanup. A refused process-group cleanup is surfaced and remains an unresolved
  descendant-cleanup gap, never a claim that descendants stopped.

## Secrets

- `chat` may read a Git-ignored `.env` for provider keys, with a bounded literal
  parse and no execution, interpolation or content-bearing diagnostic. The
  existing environment wins.
- That file is **local plaintext, never uploaded by default**, and is **not a
  secure store** or a hostile-directory sandbox.
- No authentication-file or token copying, and no implicit installs: login,
  provider configuration and remote agent policy stay with the external engine.

## Limits

- **Live consent enforcement is unresolved.** A prior attempt observed a
  completed Bash action without a pending request reaching this host, and one
  earlier ask/reject success was not repeatable. Remote policy is the engine's;
  this client forwards a human decision and owns no rule of its own.
- Remote history and interruption remain the engine's; local cancellation does
  not guarantee a remote stop.
- The default terminal uses a text-only generation endpoint — **no autonomous
  remote tool execution** — and availability, provider breadth and appearance are
  not proved.
