#!/bin/sh
# Development preview of the NEW Primeagen frame (cli/screen.c + the Determinant
# toolkit). Not part of the app build and not wired into the live view: it exists
# so the layout can be seen in a real terminal while the migration lands.
#
# Keys: 1 = HOME (menu + badge), 2 = CHAT (three columns), q = quit.
# Resize the terminal while it runs: the frame re-composes every step.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
vex=${VEXSPOKE_SOURCE_DIR:-"$root/../../host-behavior/vexspoke/src"}
hot=${HOTCWAP_SOURCE_DIR:-"$root/../../host-behavior/hotcwap"}
det=${DETERMINANT_SOURCE_DIR:-"$root/../../drivers/determinant/src"}
rel=${RELATIONAL_ENGINE_SOURCE_DIR:-"$root/../../host-behavior/relational-engine/src"}
PRIMEAGEN_ROOT="$root"
export PRIMEAGEN_ROOT
mkdir -p "$root/.build"
"${CC:-clang}" -std=gnu23 -Wall -Wextra -Werror -mcpu=apple-m1 \
    -mmacosx-version-min=14.0 -I"$root" -I"$vex" -I"$hot" -I"$det" -I"$rel" \
    "$root/cli/preview.c" "$root/cli/screen.c" "$root/cli/appearance.c" \
    "$root/tui/tui.c" "$root/tui/markdown.c" "$vex/net/json.c" \
    "$vex/image/png.c" "$rel/nio/mem.c" "$rel/type/type.c" "$vex/atomic/spin.c" \
    "$det/terminal/terminal_canvas.c" "$det/terminal/terminal_frame.c" \
    "$det/terminal/terminal_image.c" "$det/terminal/terminal_appearance.c" \
    -lncurses -lz -o "$root/.build/preview"
exec "$root/.build/preview" "$@"
