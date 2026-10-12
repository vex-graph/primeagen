#!/bin/sh
# Explicit C23 build only. No fetch/install/run. Supply VEXSPOKE_SOURCE_DIR
# for standalone checkout; output staged on same filesystem before publication.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
vex=${VEXSPOKE_SOURCE_DIR:-"$root/../../ecosystem/repos/vexspoke/src"}
api=${API_HAVEN_SOURCE_DIR:-"$root/../../ecosystem/repos/api-haven/src"}
hot=${HOTCWAP_SOURCE_DIR:-"$root/../../ecosystem/repos/hotcwap"}
mkdir -p "$root/.build"
stage=$(mktemp "$root/.build/tui.XXXXXX")
host_stage=$stage
trap 'rm -f "$stage" "$host_stage" "$root/.build/cli_view.sha256.new" "$root/.build/cli_view.sha256.raw"' EXIT HUP INT TERM
"${CC:-clang}" -std=gnu23 -Wall -Wextra -Werror -mcpu=apple-m1 \
    -mmacosx-version-min=14.0 -I"$root" -I"$vex" -I"$api" -I"$hot" \
    "$root/cli/main.c" "$root/tui/tui.c" "$root/tui/markdown.c" "$root/tui/event/mouse_event.c" \
    "$root/cli/view.c" "$root/cli/update.c" \
    "$hot/hot/hot.c" "$hot/hot/hot_trampoline.c" "$hot/hot/hot_retire.c" "$hot/hot/manifest.c" "$hot/hot/ledger.c" "$vex/security/crypto.c" \
    "$root/personality/owo.c" "$vex/net/json.c" \
    "$root/session/session_bridge.c" "$api/harness/harness.c" "$api/harness/engine_provider.c" \
    -ffunction-sections -Wl,-dead_strip -lncurses -o "$stage"
mkdir -p "$root/.build/payload"
stage=$(mktemp "$root/.build/cli_view.XXXXXX")
"${CC:-clang}" -std=gnu23 -Wall -Wextra -Werror -mcpu=apple-m1 \
    -mmacosx-version-min=14.0 -I"$root" -I"$vex" -I"$hot" -dynamiclib \
    "$root/cli/view.c" "$root/tui/tui.c" "$root/tui/markdown.c" "$vex/net/json.c" \
    -lncurses -o "$stage"
shasum -a 256 "$stage" > "$root/.build/cli_view.sha256.raw"
cut -d ' ' -f 1 "$root/.build/cli_view.sha256.raw" > "$root/.build/cli_view.sha256.new"
mv -f "$stage" "$root/.build/payload/cli_view.dylib"
mv -f "$root/.build/cli_view.sha256.new" "$root/.build/cli_view.sha256"
mv -f "$host_stage" "$root/.build/tui"
