#!/usr/bin/env python3
"""CLI-only R5 entry. No network until chat is explicitly selected.

No GUI, implicit install, unattended approvals, arbitrary shell or hidden artifact
execution. Provider/model are explicit and credentials are read from environment.
"""
"""
============================================================================
MODULE: Primeagen terminal interface (src/cli.py)
============================================================================
No agent class or persistent conversation field is owned by this CLI module.

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Terminal projection:
  - display(text) : quote control/escape bytes before terminal rendering
Consent:
  - approve(name, arguments) : show request; require literal interactive yes;
                              noninteractive input denies without execution
Lifecycle / CLI:
  - main() : explicit doctor or chat; construct borrowed provider/tools; dispatch
             prompts; /new resets history; /quit/EOF/cancel release tool artifacts

Doctor reports dependency presence without contacting providers. Chat requires
an explicit backend/model and may send retained conversation/tool results to it.
Every tool asks separately; no unattended approval or model consent flag exists.
Provider errors are reported without echoing response bodies/credentials. Tool
resources close in finally after supervised children stop. No GUI/visual claim.
============================================================================
"""
import argparse
import json
from pathlib import Path
import sys

from agent import Agent
from provider import Provider
from tools import Tools


def display(text):
    """Render untrusted text without terminal control/escape sequences."""
    return "".join(c if c in "\n\t" or (ord(c) >= 32 and not 127 <= ord(c) <= 159) else f"\\x{ord(c):02x}" for c in text)


def approve(name, arguments):
    print(display(f"Tool request: {name} {json.dumps(arguments, ensure_ascii=False)}"), file=sys.stderr)
    if not sys.stdin.isatty():
        print("Denied: approval needs an interactive terminal.", file=sys.stderr)
        return False
    return input("Allow this action? [y/N] ").strip().lower() == "y"


def main():
    parser = argparse.ArgumentParser(description="Primeagen: personal CLI agent; every tool requires consent.")
    parser.add_argument("command", choices=["chat", "doctor"])
    parser.add_argument("--provider", choices=["openai", "anthropic", "local"])
    parser.add_argument("--model")
    parser.add_argument("--base-url")
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--func", type=Path)
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--max-steps", type=int, default=16)
    args = parser.parse_args()
    if args.command == "doctor":
        root = Path(__file__).resolve().parent.parent
        print("native request binding:", "present" if (root / ".build/request").is_file() else "missing (python3 build.py)")
        func = args.func or root.parent / "func/func"
        print("Func launcher:", "present" if func.is_file() else "missing (set --func)")
        print("Providers: openai / anthropic / local. Live integration unproven until explicitly exercised.")
        return 0
    if not args.provider or not args.model:
        parser.error("chat requires --provider and --model")
    tools = None
    try:
        provider = Provider(args.provider, args.model, args.base_url, args.timeout)
        tools = Tools(args.workspace, approve, args.func, args.timeout)
        agent = Agent(provider, tools, args.max_steps)
        print("Primeagen · /new resets history · /quit exits · tools always ask permission")
        while True:
            try:
                prompt = input("you> ")
            except EOFError:
                break
            if prompt == "/quit":
                break
            if prompt == "/new":
                agent.reset()
                continue
            try:
                print("primeagen> " + display(agent.ask(prompt)))
            except (ValueError, OSError, KeyError, IndexError, TypeError, UnicodeError):
                # Provider response contents/secrets never enter error logs.
                print("[vex] primeagen: turn failed; check configuration, response format or budgets", file=sys.stderr)
        return 0
    except (ValueError, OSError) as error:
        print(f"[vex] primeagen: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130
    finally:
        if tools is not None:
            tools.close()


if __name__ == "__main__":
    raise SystemExit(main())
