#!/usr/bin/env bash
# Cross-build moonsamp.asi for GTA:SA (32-bit Windows) from Linux.
#
#   pacman -S mingw-w64-gcc     # Arch / CachyOS
#
#   ./build.sh                 # -> ./moonsamp.asi
#   OUT=dist/x.asi ./build.sh  # somewhere else
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
CC=${CC:-i686-w64-mingw32-gcc}
OUT=${OUT:-$HERE/moonsamp.asi}
VERSION=$(cat "$HERE/VERSION")

command -v "$CC" >/dev/null || { echo "$CC not found (pacman -S mingw-w64-gcc)"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# --no-insert-timestamp / --disable-auto-image-base: the same source and
# toolchain give the same bytes. Linked as moonsamp.asi whatever OUT is.
"$CC" -m32 -shared -o "$TMP/moonsamp.asi" "$HERE"/src/*.c \
    -DMOONSAMP_VERSION="\"$VERSION\"" \
    -Os -s -fno-ident -fno-asynchronous-unwind-tables \
    -Wall -Wextra \
    -ffreestanding -fno-builtin \
    -nostdlib -nostartfiles -nodefaultlibs \
    -Wl,--entry=_DllMain@12 \
    -Wl,--kill-at \
    -Wl,--no-insert-timestamp \
    -Wl,--disable-auto-image-base \
    -luser32 -lkernel32 -lgcc
cp "$TMP/moonsamp.asi" "$OUT"

echo "built $OUT"
ls -l "$OUT"
