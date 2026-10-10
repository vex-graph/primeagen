#!/bin/sh
# Explicit C23 build only. No fetch/install/run. Supply VEXSPOKE_SOURCE_DIR
# for standalone checkout; output staged on same filesystem before publication.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
vex=${VEXSPOKE_SOURCE_DIR:-"$root/../../ecosystem/repos/vexspoke/src"}
api=${API_HAVEN_SOURCE_DIR:-"$root/../../ecosystem/repos/api-haven/src"}
mkdir -p "$root/.build"
stage=$(mktemp "$root/.build/tui.XXXXXX")
trap 'rm -f "$stage"' EXIT HUP INT TERM
"${CC:-clang}" -std=gnu23 -Wall -Wextra -Werror -mcpu=apple-m1 \
    -mmacosx-version-min=14.0 -I"$root" -I"$vex" -I"$api" \
    "$root/cli/main.c" "$root/tui/tui.c" "$root/tui/markdown.c" "$root/tui/event/mouse_event.c" \
    "$root/personality/owo.c" "$vex/net/json.c" \
    "$root/session/session_bridge.c" "$api/harness/harness.c" "$api/harness/engine_provider.c" \
    -ffunction-sections -Wl,-dead_strip -lncurses -o "$stage"
mv -f "$stage" "$root/.build/tui"
