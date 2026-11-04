#!/usr/bin/env python3
"""Find SA-MP functions in other samp.dll builds from their R3-1 addresses.

    tools/find_offsets.py <dir with 0.3.7-R3-1/ 0.3.DL/ ...> <r3 rva>... [--dis]

REF=<build> takes the addresses from that build instead of R3-1.

For each R3-1 function: its first bytes, with relocated dwords and call/jmp
targets masked, are searched for in each other build; failing that, the
bytes around each R3-1 call to it are, and the call's target is read from the
match. A hit must be unique. --dis prints each match's first instructions so
it can be checked by eye; nothing here is trusted unchecked.
"""
import struct
import subprocess
import sys
from pathlib import Path

import pefile

import os
REF = os.environ.get('REF', '0.3.7-R3-1')
BUILDS = ['0.3.7-R1', '0.3.7-R2', '0.3.7-R3', '0.3.7-R3-1', '0.3.7-R4', '0.3.7-R5', '0.3.DL']


class Image:
    def __init__(self, path):
        self.path = path
        self.pe = pefile.PE(str(path), fast_load=False)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.data = self.pe.get_memory_mapped_image()
        self.relocs = set()
        for block in getattr(self.pe, 'DIRECTORY_ENTRY_BASERELOC', []):
            for e in block.entries:
                if e.type == 3:
                    self.relocs.add(e.rva)
        text = next(s for s in self.pe.sections if s.Name.startswith(b'.text'))
        self.lo = text.VirtualAddress
        self.hi = self.lo + text.Misc_VirtualSize

    def mask(self, rva, n):
        """Bytes at rva with relocations and rel32 branch targets wildcarded."""
        raw = self.data[rva:rva + n]
        m = [True] * n
        for k in range(n):
            if rva + k in self.relocs:
                for j in range(4):
                    if k + j < n:
                        m[k + j] = False
        i = 0
        while i < n:
            if raw[i] in (0xE8, 0xE9) and i + 5 <= n and m[i]:
                for j in range(1, 5):
                    m[i + j] = False
                i += 5
                continue
            i += 1
        return raw, m

    def search(self, raw, m):
        first = next(i for i in range(len(m)) if m[i])
        hits, start, d = [], self.lo, self.data
        while True:
            j = d.find(raw[first:first + 1], start, self.hi)
            if j < 0:
                break
            c = j - first
            if all(not m[k] or d[c + k] == raw[k] for k in range(len(raw))):
                hits.append(c)
            start = j + 1
        return hits

    def call_target(self, site):
        return (site + 5 + struct.unpack_from('<i', self.data, site + 1)[0]) & 0xFFFFFFFF

    def callers(self, target):
        d, out = self.data, []
        for i in range(self.lo, self.hi - 5):
            if d[i] == 0xE8 and self.call_target(i) == target:
                out.append(i)
        return out


def func_len(img, rva, cap=40):
    """Up to the first ret/int3, at most cap bytes."""
    d = img.data
    for k in range(cap):
        if d[rva + k] in (0xC3, 0xCC) or (d[rva + k] == 0xC2 and k > 0):
            return max(k + (3 if d[rva + k] == 0xC2 else 1), 8)
    return cap


def locate(ref, img, rva):
    raw, m = ref.mask(rva, func_len(ref, rva))
    hits = img.search(raw, m)
    if len(hits) == 1:
        return hits[0], 'sig'
    for before, after in ((16, 8), (32, 16), (8, 24), (40, 4), (24, 24)):
        found = set()
        for site in ref.callers(rva):
            raw, m = ref.mask(site - before, before + 5 + after)
            h = img.search(raw, m)
            if len(h) == 1:
                found.add(img.call_target(h[0] + before))
        if len(found) == 1:
            return found.pop(), 'calls'
    return None, '-'


def main():
    root = Path(sys.argv[1])
    rvas = [int(a, 16) for a in sys.argv[2:] if not a.startswith('--')]
    imgs = {b: Image(root / b / 'samp.dll') for b in BUILDS if (root / b / 'samp.dll').exists()}
    ref = imgs[REF]
    for rva in rvas:
        row = []
        for b, img in imgs.items():
            a, how = (rva, 'ref') if b == REF else locate(ref, img, rva)
            row.append((b, a, how))
        print('0x%05X  ' % rva + '  '.join('%s=%s(%s)' % (b.split('-', 1)[-1], '-' if a is None else '0x%X' % a, how)
                                         for b, a, how in row))
        if '--dis' in sys.argv:
            for b, a, how in row:
                if a is None:
                    continue
                out = subprocess.run(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn',
                                      '--start-address=0x%x' % (imgs[b].base + a),
                                      '--stop-address=0x%x' % (imgs[b].base + a + 0x18),
                                      str(imgs[b].path)], capture_output=True, text=True).stdout
                ins = [l.split('\t', 1)[1] for l in out.splitlines() if '\t' in l and ':' in l.split('\t')[0]]
                print('    %-10s %s' % (b, ' | '.join(ins[:5])))


if __name__ == '__main__':
    main()
