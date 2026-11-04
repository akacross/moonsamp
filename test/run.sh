#!/usr/bin/env bash
# moonsamp.asi against gta_sa.exe's own script engine under Wine, in a
# throwaway prefix, never the game's: host_test.c (opcode host, commands) and
# raknet_test.c (BitStreams and hooks, with samp.dll mapped but not run). The
# exe is only read; the copy that runs imports only KERNEL32 and its entry
# point only loads the test. Needs wine, python3 and i686-w64-mingw32-gcc.
#
#   test/run.sh [path/to/gta_sa.exe] [samp.dll...]
#
# Each samp.dll (any supported build) gets raknet_test and phase2_test; with
# none given, the game folder's. test/run.sh "" samp-dlls/*/samp.dll runs
# every build.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
exe="${1:-/mnt/data/Games/samp-0.3.7/gta_sa.exe}"
[ -n "$exe" ] || exe=/mnt/data/Games/samp-0.3.7/gta_sa.exe
shift || true
[ $# -gt 0 ] || set -- /mnt/data/Games/samp-0.3.7/samp.dll
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
export WINEPREFIX="$work/prefix" WINEDEBUG=-all
OUT="$work/moonsamp.asi" "$root/build.sh" >/dev/null
for t in host_test raknet_test phase2_test; do
    i686-w64-mingw32-gcc -O2 -Wall -shared -static -o "$work/$t.dll" "$here/$t.c"
done
patch_exe() {
python3 -I - "$exe" "$work/$1.exe" "$1.dll" <<'PY'
import struct, sys
d = bytearray(open(sys.argv[1], 'rb').read())
pe = struct.unpack_from('<I', d, 0x3C)[0]
nsec, optsz = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
opt, base = pe + 24, struct.unpack_from('<I', d, pe + 52)[0]
secs = [struct.unpack_from('<IIII', d, opt + optsz + 40 * i + 8) for i in range(nsec)]
def off(rva):
    return next(raw + rva - va for vsz, va, rsz, raw in secs if va <= rva < va + max(vsz, rsz))
imports = struct.unpack_from('<I', d, opt + 96 + 8)[0]
at, iat = off(imports), {}
while struct.unpack_from('<I', d, at + 12)[0]:
    oft, _, _, name, ft = struct.unpack_from('<IIIII', d, at)
    if d[off(name):off(name) + 12].upper() == b'KERNEL32.DLL':
        kernel32 = bytes(d[at:at + 20])
        for i in range(10000):
            e = struct.unpack_from('<I', d, off(oft) + 4 * i)[0]
            if not e: break
            if not e & 0x80000000: iat[bytes(d[off(e) + 2:off(e) + 40]).split(b'\0')[0]] = base + ft + 4 * i
    at += 20
first = off(imports)
d[first:first + 20] = kernel32                      # KERNEL32 alone
d[first + 20:first + 40] = bytes(20)
struct.pack_into('<II', d, opt + 96 + 8, imports, 40)
struct.pack_into('<II', d, opt + 96 + 8 * 13, 0, 0)  # no delay imports
entry = struct.unpack_from('<I', d, opt + 16)[0]
dll = sys.argv[3].encode() + b'\0'
name, proc = base + entry + 48, base + entry + 48 + len(dll)
lla, gpa, ep = iat[b'LoadLibraryA'], iat[b'GetProcAddress'], iat[b'ExitProcess']
stub = (b'\x68' + struct.pack('<I', name) + b'\xFF\x15' + struct.pack('<I', lla) + b'\x85\xC0\x74\x19'
        + b'\x68' + struct.pack('<I', proc) + b'\x50\xFF\x15' + struct.pack('<I', gpa) + b'\x85\xC0\x74\x09'
        + b'\xFF\xD0\x50\xFF\x15' + struct.pack('<I', ep)
        + b'\x6A\x03\xFF\x15' + struct.pack('<I', ep))
assert len(stub) == 48
stub += dll + b'run\0'
d[off(entry):off(entry) + len(stub)] = stub
open(sys.argv[2], 'wb').write(d)
PY
}
cd "$work"
touch moonsamp.trace    # traced, as the user runs it: its bookkeeping is tested too
status=0
run() {
    patch_exe "$1"
    echo "=== $1${2:+ ($2)}"
    rm -f moonsamp.log
    wine "$1.exe" || { status=1; failed+=("$1${2:+ $2}"); }
    echo "--- moonsamp.log"
    cat moonsamp.log 2>/dev/null || true
}
failed=()
run host_test
for dll in "$@"; do
    cp "$dll" samp.dll
    run raknet_test "$dll"
    run phase2_test "$dll"
done
[ ${#failed[@]} -eq 0 ] && echo "all passed" || printf 'FAILED: %s\n' "${failed[@]}"
exit $status
