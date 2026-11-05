#!/usr/bin/env python3
"""Check src/builds.h against each samp.dll's own code.

    tools/check_builds.py <dir with 0.3.7-R1/ ... 0.3.DL/ samp.dll>

Every value is derived again from the build's code, without the table:
functions by matching R3-1's (tools/find_offsets.py), globals and struct
offsets from the instructions that use them. A value the build's code does
not show (R1 and R2 keep some functions in their protector's section) is
listed as not checked. Needs pefile and capstone.
"""
import collections
import difflib
import re
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).parent))
import find_offsets as fo  # noqa: E402

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
ROOT = Path(__file__).parent.parent
DIRS = {'0.3.7-R1': '0.3.7-R1', '0.3.7-R2': '0.3.7-R2', '0.3.7-R3': '0.3.7-R3', '0.3.7-R3-1': '0.3.7-R3-1',
        '0.3.7-R4': '0.3.7-R4', '0.3.7-R5': '0.3.7-R5', '0.3.DL-R1': '0.3.DL'}
POOLS = ['player', 'vehicle', 'gangzone', 'label', 'textdraw', 'object', 'pickup', 'actor', 'menu']
SIZES = {0x2F3E: 'player', 0x17898: 'vehicle', 0x2000: 'gangzone', 0x10800: 'label', 0x4800: 'textdraw',
         0x1F44: 'object', 0x41A4: 'object', 0x23004: 'pickup', 0x4E24: 'actor', 0x402: 'menu'}
FUNCS = ['add_message', 'local_chat', 'send_command', 'player_color', 'handle_rpc', 'encode_string',
         'decode_string', 'bs_ctor', 'bs_dtor', 'dialog_show', 'dialog_close', 'listbox_selected',
         'listbox_select', 'editbox_get', 'editbox_set', 'set_cursor_mode', 'vehicle_find', 'special_action',
         'textdraw_set_text', 'update_players', 'current_anim', 'anim_entry', 'process_input',
         'local_spawn', 'local_set_special', 'input_open', 'input_close', 'scoreboard_enable', 'scoreboard_close',
         'td_create', 'td_delete', 'label_create', 'label_delete', 'send_unoccupied', 'send_onfoot', 'send_aim',
         'send_trailer', 'send_passenger', 'send_stats', 'update_weapons', 'send_incar']
GLOBALS = ['netgame', 'chat', 'input', 'dialog', 'game', 'scoreboard', 'death_window']
# R3-1 functions read for the offsets
INIT_POOLS, FIND, REMOVE, GET_SCORE, IS_NPC, GET_NAME = 0x81D0, 0x13570, 0x16E70, 0x6E0E0, 0xE130, 0x16F00
LOCAL_PROCESS, CONNECT, CONNECTION_SUCCEEDED = 0x7270, 0x85D0, 0xAA20
LOCAL_GET_PED, ENABLE_SPECTATING, INCAR_SEND_RATE, CONNECTION_LOST, UNOCCUPIED_SYNC = 0x2D50, 0x4010, 0x3970, 0xA990, 0x96D0
STRING_COMPRESSOR_USER, OBJECT_FIND, ONFOOT_SEND_RATE = 0x53F80, 0x12680, 0x39B0
GLOBAL_USERS = [0x85D0, 0x8BA0, 0xA1E0, 0x6F8C0, 0x9FFE0, 0x679F0, 0x69190, 0x6E5C0, 0xB270, 0x16110, 0x5820,
                0x6F140, 0xC5E00, 0x1E0E0, 0xC47D0, 0xC4A30, 0x6C630]
SET_NAME = bytes.fromhex('568b742408' '8bc6' '57' '8d7801')      # push esi; strlen of the argument
SHUTDOWN = bytes.fromhex('53555657' '33db' '33ff' '8bf1' '33ed')  # ShutdownForRestart's prologue
# constants in the source: RakPeer below RakClient's RakClientInterface (samp.c), its RPC nodes (info.c)
RAKPEER, RPC_NODES = 0xDDE, 0x801


def parse_table():
    text = (ROOT / 'src' / 'builds.h').read_text()
    macros = {m.group(1): [int(x, 0) for x in m.group(2).split(',')]
              for m in re.finditer(r'#define (\w+)\s+\{([^}]*)\}', text)}
    out = {}
    for block in re.findall(r'\{\s*(\.name = .*?)\n    \}', text, re.S):
        e = {}
        for k, v in re.findall(r'\.(\w+) = ([^,]+),', block + ','):
            v = v.strip()
            e[k] = v.strip('"') if v.startswith('"') else macros[v] if v in macros else int(v, 0)
        out[e['name']] = e
    return out


def ins(img, rva, n=0x400, stop=('ret',)):
    out = []
    for i in md.disasm(bytes(img.data[rva:rva + n]), rva):
        out.append(i)
        if i.mnemonic in stop:
            break
    return out


MEM = re.compile(r'\[(e\w\w)(?: \+ (e\w\w)(?:\*\d)?)?(?: \+ (0x[0-9a-f]+|\d+))?\]')


def mem(op):
    """(base register, index register or None, displacement) of a memory operand."""
    m = MEM.search(op)
    return (m.group(1), m.group(2), int(m.group(3), 0) if m.group(3) else 0) if m else None


def reads(img, rva, n=0x80, stop=('ret',)):
    """The memory operands not on the stack, in order."""
    out = []
    for i in ins(img, rva, n, stop):
        r = mem(i.op_str)
        if r and r[0] not in ('esp', 'ebp'):
            out.append(r)
    return out


def norm(i):
    return i.mnemonic + ' ' + re.sub(r'0x[0-9a-f]+|\b\d+\b', 'N', i.op_str)


class Build:
    def __init__(self, ref, imgs, img):
        self.ref, self.imgs, self.img = ref, imgs, img

    def locate(self, rva):
        """R3-1's function in this build: matched from R3-1, else from any
        build R3-1's matches."""
        a, _ = fo.locate(self.ref, self.img, rva)
        if a is not None:
            return a
        for other in self.imgs:
            if other is self.img or other is self.ref:
                continue
            b, _ = fo.locate(self.ref, other, rva)
            if b is not None:
                a, _ = fo.locate(other, self.img, b)
                if a is not None:
                    return a
        return None

    def find(self, pattern):
        d = bytes(self.img.data[self.img.lo:self.img.hi])
        return [self.img.lo + m.start() for m in re.finditer(re.escape(pattern), d)]


def globals_of(b, row31):
    """R3-1's global object pointers through the aligned instructions of
    functions that read them; a majority vote over every use."""
    want = {b.ref.base + row31[g]: g for g in GLOBALS}
    votes = collections.defaultdict(collections.Counter)
    for rva in GLOBAL_USERS:
        a = b.locate(rva)
        if a is None:
            continue
        x, y = ins(b.ref, rva, 0x800, ()), ins(b.img, a, 0x800, ())
        sm = difflib.SequenceMatcher(None, [norm(i) for i in x], [norm(i) for i in y], autojunk=False)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag != 'equal':
                continue
            for k in range(i2 - i1):
                for p, q in zip(re.findall(r'0x[0-9a-f]+', x[i1 + k].op_str), re.findall(r'0x[0-9a-f]+', y[j1 + k].op_str)):
                    if int(p, 16) in want:
                        votes[want[int(p, 16)]][int(q, 16) - b.img.base] += 1
    return {g: votes[g].most_common(1)[0][0] for g in GLOBALS if votes[g]}


def after_global(b, rva, g):
    """In function rva, the displacement of the first use of the register just
    loaded from global g."""
    seq = ins(b.img, rva, 0x200)
    for k, i in enumerate(seq):
        m = re.match(r'(e\w\w), dword ptr \[(0x[0-9a-f]+)\]$', i.op_str)
        if i.mnemonic == 'mov' and m and int(m.group(2), 16) - b.img.base == g:
            reg = m.group(1)
            for j in seq[k + 1:]:
                r = mem(j.op_str)
                if r and r[0] == reg:
                    return r[2]
    return None


def derive(b, row31):
    img, d = b.img, {}
    d['entry'] = img.pe.OPTIONAL_HEADER.AddressOfEntryPoint
    for f in FUNCS:
        d[f] = b.locate(row31[f])
    d.update(globals_of(b, row31))

    # InitializePools: each pool is allocated (`push size`) and stored after the
    # next allocation; the first store is the SPools pointer itself
    news, ats = [], []
    for i in ins(img, b.locate(INIT_POOLS)):
        if i.mnemonic == 'push' and i.op_str.startswith('0x') and int(i.op_str, 16) in SIZES:
            news.append(SIZES[int(i.op_str, 16)])
        if i.mnemonic == 'mov' and i.op_str.endswith(', eax') and i.op_str.startswith('dword ptr ['):
            r = mem(i.op_str)
            if r and r[0] not in ('esp', 'ebp'):
                ats.append(r[2])
    d['ng_pools'] = ats[0]
    slots = {news[k - 1]: ats[k] for k in range(1, min(len(ats), len(news) + 1))}
    d['pool'] = [slots.get(p) for p in POOLS]

    # CPlayerPool::Find: largest, used, info, remote, ped (an indexed read
    # without a displacement only forms the address)
    r = [x for x in reads(img, b.locate(FIND)) if not (x[1] and x[2] == 0)]
    d['pp_largest'], d['pp_used'], d['pp_info'], d['info_remote'], d['remote_ped'] = (x[2] for x in r[:5])

    # ShutdownForRestart: unique prologue; it stores the restarting state, and
    # reads the local player through the player pool
    restarting = None
    hits = b.find(SHUTDOWN)
    if len(hits) == 1:
        d['shutdown_restart'] = a = hits[0]
        for i in ins(img, a, 0x100, ()):
            m = re.match(r'dword ptr \[esi \+ (0x[0-9a-f]+)\], (0x[0-9a-f]+|\d+)$', i.op_str)
            if i.mnemonic == 'mov' and m:
                d['ng_state'], restarting = int(m.group(1), 16), int(m.group(2), 0)
                break
        d['pp_local_obj'] = reads(img, a, 0x100, ())[7][2]

    # CRemotePlayer::Remove -> ResetData
    call = next(int(i.op_str, 16) for i in ins(img, b.locate(REMOVE), 0x20) if i.mnemonic == 'call')
    rs = ins(img, call, 0x200)
    zero32 = [mem(i.op_str)[2] for i in rs if i.mnemonic == 'mov' and i.op_str.startswith('dword ptr [esi') and i.op_str.endswith(', ebx')]
    zero8 = [mem(i.op_str)[2] for i in rs if i.mnemonic == 'mov' and i.op_str.startswith('byte ptr [esi') and i.op_str.endswith(', bl')]
    d['remote_armour'] = zero32[1]
    d['remote_special'] = zero8[1]
    d['remote_status'] = next(mem(i.op_str)[2] for i in rs if i.op_str.startswith('dword ptr [esi') and i.op_str.endswith(', 2'))
    # its sync data: on-foot, in-car, passenger, aim, in the order it clears them
    leas = [mem(i.op_str)[2] for i in rs if i.mnemonic == 'lea' and re.match(r'e(di|cx|ax), \[esi', i.op_str)]
    d['remote_onfoot'], d['remote_incar'], d['remote_passenger'], d['remote_aim'] = leas[:4]

    # score, ping, local score, local ping: four getters in this order
    g = [x[2] for x in reads(img, b.locate(GET_SCORE), 0x80, ())]
    d['info_score'], d['info_ping'], d['pp_local_score'], d['pp_local_ping'] = g[1], g[3], g[4], g[5]
    d['info_npc'] = reads(img, b.locate(IS_NPC), 0x40, ('int3',))[-1][2]

    # CPlayerPool::GetName: local id, then the remote's name buffer
    a = b.locate(GET_NAME)
    if a is not None:
        seq = ins(img, a, 0x60, ('int3',))
        ids = [mem(i.op_str)[2] for i in seq if i.mnemonic == 'cmp' and 'word ptr [ecx' in i.op_str]
        names = [mem(i.op_str)[2] for i in seq if i.mnemonic == 'lea' and i.op_str.startswith('eax, [ecx')]
        if ids and len(names) == 2:
            d['pp_local_id'], d['info_name'] = ids[0], names[1]

    # SetLocalPlayerName: the first strlen-and-assign at pool + X; the name's
    # buffer is at X + 4
    for a in b.find(SET_NAME):
        add = [i for i in ins(img, a, 0x30) if i.mnemonic == 'add' and re.match(r'ecx, (0x[0-9a-f]+|\d+)$', i.op_str)]
        if add:
            d['set_local_name'], d['pp_local_name'] = a, int(add[0].op_str.split(', ')[1], 0) + 4
            break

    d['local_ped'] = reads(img, b.locate(LOCAL_GET_PED), 0x10)[0][2]
    # active: cleared by EnableSpectating; else CLocalPlayer::Process clears it
    # and sets "wasted" right after it
    a = b.locate(ENABLE_SPECTATING)
    if a is not None:
        d['local_active'] = next(mem(i.op_str)[2] for i in ins(img, a, 0x80)
                                 if i.mnemonic == 'mov' and i.op_str.startswith('dword ptr [esi') and i.op_str.endswith(', ebx'))
    elif (a := b.locate(LOCAL_PROCESS)) is not None:
        stores = [(mem(i.op_str)[2], i.op_str.split(', ')[-1]) for i in ins(img, a, 0x1000, ())
                  if i.mnemonic == 'mov' and i.op_str.startswith('dword ptr [esi')]
        d['local_active'] = next((x for x, v in stores if v == 'ebp' and (x + 4, '1') in stores), None)
    d['game_cursor'] = next(mem(i.op_str)[2] for i in ins(img, d['set_cursor_mode'])
                            if i.op_str.endswith(', 2') and '[esi' in i.op_str)

    # CNetGame: RakClient as CLocalPlayer::Chat reads it, the host from the
    # LAN flag (host + 0x209) the send rates read
    if 'netgame' in d:
        d['ng_rakclient'] = after_global(b, d['local_chat'], d['netgame'])
        a = b.locate(INCAR_SEND_RATE)
        lan = after_global(b, a, d['netgame']) if a is not None else None
        d['ng_host'] = lan - 0x209 if lan else None

    # game states, where the build's code shows them
    states = [None] * 5
    if 'ng_state' in d:
        states[4] = restarting
        st = re.compile(r'dword ptr \[e\w\w \+ %s\], (0x[0-9a-f]+|\d+)$' % hex(d['ng_state']))
        for rva, k in ((CONNECTION_LOST, 0), (CONNECT, 1), (UNOCCUPIED_SYNC, 2), (CONNECTION_SUCCEEDED, 3)):
            a = b.locate(rva)
            for i in ins(img, a, 0x200) if a is not None else []:
                m = st.search(i.op_str)
                if m:
                    states[k] = int(m.group(1), 0)
                    break
    d['state'] = states

    # CObjectPool::Find walks the array from id 1
    lea = next(i for i in ins(img, b.locate(OBJECT_FIND), 0x30) if i.mnemonic == 'lea' and i.op_str.startswith('esi'))
    d['obj_objects'] = mem(lea.op_str)[2] - 4

    # the send rates: the global GetOnfootSendRate reads after CNetGame's
    a = b.locate(ONFOOT_SEND_RATE)
    g = [int(x, 16) - img.base for i in ins(img, a, 0x40, ('int3',)) for x in re.findall(r'\[(0x[0-9a-f]+)\]', i.op_str)] if a else []
    d['sendrates'] = next((x for x in g if x not in (d.get('netgame'), d.get('game'))), None)

    a = b.locate(STRING_COMPRESSOR_USER)
    d['string_compressor'] = next(int(i.op_str.split('[0x')[1].split(']')[0], 16) - img.base
                                  for i in ins(img, a, 0x30) if '[0x' in i.op_str)
    callers = img.callers(d['handle_rpc']) if d['handle_rpc'] else []
    d['handle_rpc_call'] = callers[0] if len(callers) == 1 else None
    # SA-MP's IDirect3DDevice9 proxy forwards each method as mov eax, [device]; mov ecx, [eax]
    stubs = collections.Counter(int.from_bytes(m.group(1), 'little') - img.base
                                for m in re.finditer(rb'\xA1(....)\x8B\x08', img.data[img.lo:img.hi], re.S))
    top = stubs.most_common(2)
    d['device'] = top[0][0] if top and top[0][1] > 100 and (len(top) < 2 or top[1][1] < 50) else None
    return d


def constants(img, d):
    """The RakClient factory's `add eax, X` and the interface's `sub ecx, X` thunks give RakPeer's place;
    HandleRPCPacket's `add esi, X` before the node lookups gives the RPC nodes."""
    text = img.data[img.lo:img.hi]
    adds = {int.from_bytes(m.group(1), 'little') for m in re.finditer(rb'\x85\xC0\x74.\x05(....)', text, re.S)}
    subs = collections.Counter(int.from_bytes(m.group(1), 'little') for m in re.finditer(rb'\x81\xE9(....)\xE9', text, re.S))
    peer = [x for x in adds if subs[x] > 8]
    nodes = [int(i.op_str.split(', ')[1], 16) for i in ins(img, d['handle_rpc'], 0x100)
             if i.mnemonic == 'add' and i.op_str.startswith('esi, 0x')] if d['handle_rpc'] else []
    return {'rakpeer': peer[0] if len(peer) == 1 else None, 'rpc_nodes': nodes[0] if len(nodes) == 1 else None}


def fmt(v):
    if isinstance(v, list):
        return '{' + ', '.join('-' if x is None else hex(x) for x in v) + '}'
    return '-' if v is None else hex(v)


def main():
    root = Path(sys.argv[1])
    table = parse_table()
    imgs = {name: fo.Image(root / DIRS[name] / 'samp.dll') for name in table}
    ref = imgs['0.3.7-R3-1']
    bad = 0
    for name, row in table.items():
        b = Build(ref, list(imgs.values()), imgs[name])
        got = derive(b, table['0.3.7-R3-1'])
        problems, unchecked = [], []
        got.update(constants(imgs[name], got))
        for k, want in list(row.items()) + [('rakpeer', RAKPEER), ('rpc_nodes', RPC_NODES)]:
            if k == 'name':
                continue
            v = got.get(k)
            if k == 'state':
                if any(x is not None and x != w for x, w in zip(v, want)):
                    problems.append('state: table %s, code %s' % (fmt(want), fmt(v)))
                if None in v:
                    unchecked.append('state[%s]' % ','.join(str(i) for i, x in enumerate(v) if x is None))
            elif v is None:
                unchecked.append(k)
            elif v != want:
                problems.append('%s: table %s, code %s' % (k, fmt(want), fmt(v)))
        print('%-11s %s%s' % (name, 'ok' if not problems else 'MISMATCH',
                              '  (not shown by its code: %s)' % ', '.join(unchecked) if unchecked else ''))
        for p in problems:
            print('    ' + p)
        bad += len(problems)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
