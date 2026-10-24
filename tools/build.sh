#!/bin/sh
# Explicit strict native tools build; never downloads, installs or executes tools.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
vex=${VEXSPOKE_SOURCE_DIR:-"$root/../../ecosystem/vexspoke/src"}
mkdir -p "$root/.build"
stage=$(mktemp "$root/.build/tools.XXXXXX")
trap 'rm -f "$stage"' EXIT HUP INT TERM
"${CC:-clang}" -std=gnu23 -Wall -Wextra -Werror -mcpu=apple-m1 \
    -mmacosx-version-min=14.0 -I"$root" -I"$vex" \
    "$root/tools/main.c" "$root/tools/fs.c" "$root/tools/read.c" "$root/tools/list.c" \
    "$root/tools/write.c" "$root/tools/create.c" "$root/tools/edit.c" "$root/tools/delete.c" \
    "$root/tools/bash.c" "$root/tools/websearch.c" "$vex/net/json.c" -o "$stage"
mv -f "$stage" "$root/.build/tools"
