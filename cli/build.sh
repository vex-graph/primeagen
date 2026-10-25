#!/bin/sh
# Explicit C23 build only. No fetch/install/run. Supply VEXSPOKE_SOURCE_DIR
# for standalone checkout; output staged on same filesystem before publication.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
vex=${VEXSPOKE_SOURCE_DIR:-"$root/../../ecosystem/vexspoke/src"}
api=${API_HAVEN_SOURCE_DIR:-"$root/../../ecosystem/api-haven/src"}
hot=${HOTCWAP_SOURCE_DIR:-"$root/../../ecosystem/hotcwap"}
det=${DETERMINANT_SOURCE_DIR:-"$root/../../ecosystem/determinant/src"}
rel=${RELATIONAL_ENGINE_SOURCE_DIR:-"$root/../../ecosystem/relational-engine/src"}
# PRIMEAGEN_ROOT lets the host find its own resources/w.png in any checkout.
PRIMEAGEN_ROOT="$root"
export PRIMEAGEN_ROOT
# The R3 terminal toolkit (canvas/view/appearance). Determinant stays bare; the
# app supplies every string, colour and glyph choice.
det_src="$det/ui/surface/terminal_canvas.c $det/ui/panel/terminal_frame.c $det/ui/image/terminal_image.c $det/ui/theme/terminal_appearance.c $det/ui/tree/terminal_tree_view.c"
sh "$root/tools/build.sh"
mkdir -p "$root/.build"
stage=$(mktemp "$root/.build/tui.XXXXXX")
host_stage=$stage
trap 'rm -f "$stage" "$host_stage" "$root/.build/cli_view.sha256.new" "$root/.build/cli_view.sha256.raw"' EXIT HUP INT TERM
"${CC:-clang}" -std=gnu23 -Wall -Wextra -Werror -mcpu=apple-m1 \
    -mmacosx-version-min=14.0 -I"$root" -I"$vex" -I"$api" -I"$hot" -I"$det" -I"$rel" \
    "$root/cli/main.c" "$root/tui/tui.c" "$root/tui/markdown.c" "$root/tui/event/mouse_event.c" \
    "$root/cli/view.c" "$root/cli/appearance.c" "$root/cli/screen.c" "$root/cli/update.c" "$root/src/settings.c" \
    $det_src "$vex/image/png.c" "$rel/nio/mem.c" "$rel/type/type.c" "$vex/atomic/spin.c" \
    "$hot/hot/hot.c" "$hot/hot/hot_trampoline.c" "$hot/hot/hot_retire.c" "$hot/hot/manifest.c" "$hot/hot/ledger.c" "$vex/security/crypto.c" \
    "$root/personality/owo.c" "$vex/net/json.c" \
    "$root/session/session_bridge.c" "$api/harness/harness.c" "$api/harness/engine_provider.c" \
    -ffunction-sections -Wl,-dead_strip -lncurses -lz -o "$stage"
mkdir -p "$root/.build/payload"
stage=$(mktemp "$root/.build/cli_view.XXXXXX")
"${CC:-clang}" -std=gnu23 -Wall -Wextra -Werror -mcpu=apple-m1 \
    -mmacosx-version-min=14.0 -I"$root" -I"$vex" -I"$hot" -I"$det" -I"$rel" -dynamiclib \
    "$root/cli/view.c" "$root/cli/appearance.c" "$root/cli/screen.c" "$root/tui/tui.c" "$root/tui/markdown.c" "$vex/net/json.c" \
    $det_src "$rel/nio/mem.c" "$rel/type/type.c" "$vex/atomic/spin.c" \
    -lncurses -o "$stage"
shasum -a 256 "$stage" > "$root/.build/cli_view.sha256.raw"
cut -d ' ' -f 1 "$root/.build/cli_view.sha256.raw" > "$root/.build/cli_view.sha256.new"
mv -f "$stage" "$root/.build/payload/cli_view.dylib"
mv -f "$root/.build/cli_view.sha256.new" "$root/.build/cli_view.sha256"
mv -f "$host_stage" "$root/.build/tui"
