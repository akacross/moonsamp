#!/usr/bin/env bash
# Build the drop-in release zip from a git ref.
#
#   tools/package.sh v0.4.9      # -> dist/moonsamp.zip
#   tools/package.sh             # HEAD (for a dry run)
#
# The ASI is built from the ref's committed files only (git archive), never
# from the working tree, so the zip matches what the tag says it is. The zip
# holds what a player extracts next to gta_sa.exe:
#
#   moonsamp.asi           the plugin
#   moonsamp-LICENSE.txt   GPL-3.0 (renamed so it can't clash with another
#                          mod's LICENSE in the game folder)
#   moonloader/lib/moonsamp.lua     moonsamp_run for Lua, which these two use:
#   moonloader/lib/newopcodes.lua   the NewOpcodes functions for scripts
#   moonloader/lib/cleo5.lua        CLEO 5's own opcodes for scripts
#
# Entry times are the ref's commit time, so rebuilding a tag with the same
# toolchain gives the same zip.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REF="${1:-HEAD}"

git -C "$ROOT" rev-parse --verify --quiet "$REF^{commit}" >/dev/null \
    || { echo "no such ref: $REF"; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir "$WORK/src"
git -C "$ROOT" archive "$REF" | tar -x -C "$WORK/src"
OUT="$WORK/moonsamp.asi" bash "$WORK/src/build.sh" >/dev/null

STAMP="$(git -C "$ROOT" log -1 --format=%cd --date=format:'%Y %m %d %H %M %S' "$REF")"
mkdir -p "$ROOT/dist"
ZIP="$ROOT/dist/moonsamp.zip"

python3 - "$ZIP" "$STAMP" \
    "moonsamp.asi=$WORK/moonsamp.asi" \
    "moonsamp-LICENSE.txt=$WORK/src/LICENSE" \
    "moonloader/lib/moonsamp.lua=$WORK/src/moonloader/lib/moonsamp.lua" \
    "moonloader/lib/newopcodes.lua=$WORK/src/moonloader/lib/newopcodes.lua" \
    "moonloader/lib/cleo5.lua=$WORK/src/moonloader/lib/cleo5.lua" <<'PY'
import sys, zipfile
out, stamp, *entries = sys.argv[1:]
when = tuple(int(x) for x in stamp.split())
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for entry in entries:
        name, path = entry.split('=', 1)
        info = zipfile.ZipInfo(name, date_time=when)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        z.writestr(info, open(path, 'rb').read())
PY

echo "packaged $REF -> $ZIP"
python3 - "$ZIP" <<'PY'
import hashlib, os, sys, zipfile
path = sys.argv[1]
for i in zipfile.ZipFile(path).infolist():
    data = zipfile.ZipFile(path).read(i)
    print(f"  {i.filename:32} {len(data):6} bytes  sha256 {hashlib.sha256(data).hexdigest()}")
data = open(path, 'rb').read()
print(f"  {os.path.basename(path):32} {len(data):6} bytes  sha256 {hashlib.sha256(data).hexdigest()}")
PY
