#!/usr/bin/env python3
"""Compare a SAMPFUNCS self-test log with a moonsamp one.

    test/ingame/compare.py moonsamp_selftest-sampfuncs.log moonsamp_selftest-moonsamp.log

CHECK lines must be "ok" in both. VALUE lines are paired by label and sorted
into: same, different, missing in moonsamp (SAMPFUNCS returns something,
moonsamp has no such function or returns nothing), and only one side has it.
Dynamic values (ping, health, positions, other players) differ between two
sessions anyway; read the "different" list with that in mind.
"""
import re
import sys


def load(path):
    checks, values = {}, {}
    for line in open(path, encoding='utf-8', errors='replace'):
        m = re.match(r'(CHECK|VALUE) (.{48}) (.*)$', line.rstrip('\n'))
        if not m:
            continue
        kind, label, rest = m.group(1), m.group(2).strip(), m.group(3)
        (checks if kind == 'CHECK' else values)[label] = rest
    return checks, values


def empty(v):
    return v in ('nil-fn', '', 'nil', 'false', '0', '""', 'false, 0', 'false, -1', 'false, nil') or v.startswith('error')


sf_c, sf_v = load(sys.argv[1])
ms_c, ms_v = load(sys.argv[2])

print('== CHECK failures')
bad = [(l, sf_c.get(l), ms_c.get(l)) for l in sorted(set(sf_c) | set(ms_c))
       if not (sf_c.get(l, '').startswith('ok') and ms_c.get(l, '').startswith('ok'))]
for l, a, b in bad:
    print(f'  {l}\n      sampfuncs: {a}\n      moonsamp:  {b}')
print(f'  {len(bad)} of {len(set(sf_c) | set(ms_c))}')

same, diff, missing, onesided = [], [], [], []
for l in sorted(set(sf_v) | set(ms_v)):
    a, b = sf_v.get(l), ms_v.get(l)
    if a is None or b is None:
        onesided.append((l, a, b))
    elif a == b:
        same.append(l)
    elif empty(b) and not empty(a):
        missing.append((l, a, b))
    else:
        diff.append((l, a, b))

for title, rows in (('missing in moonsamp', missing), ('different', diff), ('only one side', onesided)):
    print(f'\n== {title} ({len(rows)})')
    for l, a, b in rows:
        print(f'  {l}\n      sampfuncs: {a}\n      moonsamp:  {b}')
print(f'\n== same: {len(same)}')
