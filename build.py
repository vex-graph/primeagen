#!/usr/bin/env python3
"""Explicit strict native binding build; no fetch, install or program run.

Standalone consumers supply source roots; workspace paths are discovery defaults,
not an ecosystem target dependency. Missing headers are real build failures.
"""
"""
============================================================================
MODULE: Primeagen explicit native build (build.py)
============================================================================
No runtime class or persistent field is owned by this procedural build entry.

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Build operation:
  - build(output, compiler, sanitizer)
      Resolve explicit dependency roots; compile real Message/chat/JSON sources;
      use strict baseline flags; publish completed binding by atomic replacement.
CLI entry:
  - __main__ parser : --output / --compiler / --sanitizer, then explicit build

Workspace paths are discovery defaults only. No fetch, fake declaration, app run,
provider contact or installation occurs. Unused native HTTP operations are
linker-discarded, not presented as working TLS. The compiler and output parent
are trusted. Failed compiler stages preserve existing output and reclaim private
temporary state. Sanitizers instrument C clients, not Python or provider servers.
============================================================================
"""
import argparse
import os
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))
from process import run

ROOT = Path(__file__).resolve().parent
WORKSPACE = ROOT.parent.parent


def build(output, compiler="clang", sanitizer=False):
    """Compile actual dependency sources, dropping unused HTTP send operations."""
    api = Path(os.environ.get("API_HAVEN_SOURCE_DIR", WORKSPACE / "ecosystem/repos/api-haven/src"))
    harness = Path(os.environ.get("HARNESS_SOURCE_DIR", WORKSPACE / "ecosystem/repos/harness/src"))
    spoke = Path(os.environ.get("VEXSPOKE_SOURCE_DIR", WORKSPACE / "ecosystem/repos/vexspoke/src"))
    flags = ["-std=gnu23", "-Wall", "-Wextra", "-Werror", "-O2", "-ffunction-sections", "-fdata-sections"]
    if sys.platform == "darwin":
        flags += ["-arch", "arm64", "-mcpu=apple-m1", "-mmacosx-version-min=14.0", "-Wl,-dead_strip"]
    else:
        flags += ["-Wl,--gc-sections"]
    if sanitizer:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    sources = [ROOT / "native/request.c", harness / "space/message.c",
               api / "ai/ai_chat.c", api / "ai/ai_chat_anthropic.c",
               api / "ai/ai_provider.c", api / "api/auth.c", spoke / "net/json.c"]
    output = Path(output).absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.is_symlink() or output.is_dir():
        raise ValueError("invalid build output")
    with tempfile.TemporaryDirectory(prefix=".primeagen-build-", dir=output.parent) as temp:
        artifact = Path(temp) / "request"
        argv = [compiler, *flags, "-I", str(api), "-I", str(harness), "-I", str(spoke),
                *(str(source) for source in sources), "-o", str(artifact)]
        run(argv, timeout=120)
        os.replace(artifact, output)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / ".build/request")
    parser.add_argument("--compiler", default="clang")
    parser.add_argument("--sanitizer", action="store_true")
    args = parser.parse_args()
    try:
        print(build(args.output, args.compiler, args.sanitizer))
    except (OSError, ValueError) as error:
        print(f"[vex] primeagen build: {error}", file=sys.stderr)
        raise SystemExit(1)
