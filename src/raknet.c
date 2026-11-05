/* RakNet: BitStreams, the four hooks behind onSendRpc / onSendPacket /
 * onReceiveRpc / onReceivePacket, and sending or emulating RPCs and packets.
 *
 * A BitStream is RakNet's own layout, so samp.dll can read the ones scripts
 * build. samp.dll frees BitStream buffers with its own CRT, so moonsamp never
 * grows one of samp.dll's: hooks hand scripts a copy, and anything samp.dll
 * writes into goes through samp.dll's own constructor and destructor.
 *
 * Hooks: RakClientInterface vtable slots Send (6), Receive (8),
 * DeallocatePacket (9) and RPC (25), and the call to RakPeer::HandleRPCPacket
 * in RakClient::Receive, which parses RPCs before any handler runs. */
#include "moonsamp.h"
#include "names.h"

typedef struct BitStream {
    int bits_used;
    int bits_allocated;
    int read_offset;
    uint8_t *data;
    uint8_t copy_data;
    uint8_t stack_data[256];
} __attribute__((packed)) BitStream;

typedef struct Packet {
    uint16_t player_index;
    uint32_t player_addr;
    uint16_t player_port;
    uint32_t length;
    uint32_t bit_size;
    uint8_t *data;
    uint8_t delete_data;
} __attribute__((packed)) Packet;

#define ID_RPC        20
#define ID_TIMESTAMP  40
#define STACK_BITS    (256 * 8)

enum { OUT_RPC, OUT_PACKET, IN_RPC, IN_PACKET, HOOK_TYPES };
enum { BS_BYTE, BS_BOOL, BS_SHORT, BS_INT, BS_FLOAT, BS_ARRAY, BS_BITSTREAM };

/* ---- BitStream, as RakNet 2.x writes it -------------------------------- */

static void *mem_alloc(size_t n) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n ? n : 1); }
static void mem_free(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }

static void bs_init(BitStream *bs)
{
    memset(bs, 0, sizeof *bs);
    bs->bits_allocated = STACK_BITS;
    bs->data = bs->stack_data;
    bs->copy_data = 1;
}

static BitStream *bs_new(void)
{
    BitStream *bs = mem_alloc(sizeof *bs);
    bs_init(bs);
    return bs;
}

static void bs_release(BitStream *bs)
{
    if (bs->copy_data && bs->data != bs->stack_data)
        mem_free(bs->data);
    bs->data = bs->stack_data;
}

static void bs_delete(BitStream *bs)
{
    if (!bs)
        return;
    bs_release(bs);
    mem_free(bs);
}

static void bs_reserve(BitStream *bs, int add)
{
    int need = bs->bits_used + add;
    if (need <= 0 || ((bs->bits_allocated - 1) >> 3) >= ((need - 1) >> 3))
        return;
    {
        int bytes = (need * 2 + 7) >> 3;
        uint8_t *p = mem_alloc(bytes);
        memcpy(p, bs->data, (bs->bits_allocated + 7) >> 3 < bytes ? (bs->bits_allocated + 7) >> 3 : bytes);
        if (bs->data != bs->stack_data && bs->copy_data)
            mem_free(bs->data);
        bs->data = p;
        bs->copy_data = 1;
        bs->bits_allocated = bytes * 8;
    }
}

static void bs_write_bits(BitStream *bs, const uint8_t *in, int n, int right_aligned)
{
    int off = 0, mod;
    if (n <= 0)
        return;
    bs_reserve(bs, n);
    mod = bs->bits_used & 7;
    while (n > 0) {
        uint8_t b = in[off];
        if (n < 8 && right_aligned)
            b <<= 8 - n;
        if (mod == 0) {
            bs->data[bs->bits_used >> 3] = b;
        } else {
            bs->data[bs->bits_used >> 3] |= b >> mod;
            if (8 - mod < n)
                bs->data[(bs->bits_used >> 3) + 1] = (uint8_t)(b << (8 - mod));
        }
        bs->bits_used += n >= 8 ? 8 : n;
        n -= 8;
        off++;
    }
}

static void bs_write_bit(BitStream *bs, int v)
{
    int mod;
    bs_reserve(bs, 1);
    mod = bs->bits_used & 7;
    if (mod == 0)
        bs->data[bs->bits_used >> 3] = v ? 0x80 : 0;
    else if (v)
        bs->data[bs->bits_used >> 3] |= 0x80 >> mod;
    bs->bits_used++;
}

static int bs_read_bits(BitStream *bs, uint8_t *out, int n, int align_right)
{
    int mod, off = 0;
    if (n <= 0 || bs->read_offset + n > bs->bits_used)
        return 0;
    mod = bs->read_offset & 7;
    memset(out, 0, (n + 7) >> 3);
    while (n > 0) {
        out[off] |= (uint8_t)(bs->data[bs->read_offset >> 3] << mod);
        if (mod > 0 && n > 8 - mod)
            out[off] |= bs->data[(bs->read_offset >> 3) + 1] >> (8 - mod);
        n -= 8;
        if (n < 0) {
            if (align_right)
                out[off] >>= -n;
            bs->read_offset += 8 + n;
        } else {
            bs->read_offset += 8;
        }
        off++;
    }
    return 1;
}

static int bs_read_bit(BitStream *bs, int *v)
{
    if (bs->read_offset >= bs->bits_used)
        return 0;
    *v = (bs->data[bs->read_offset >> 3] & (0x80 >> (bs->read_offset & 7))) != 0;
    bs->read_offset++;
    return 1;
}

static void bs_write_bytes(BitStream *bs, const void *p, int n) { bs_write_bits(bs, p, n * 8, 1); }

/* WriteCompressed / ReadCompressed of an unsigned 32-bit value. */
static void bs_write_compressed(BitStream *bs, uint32_t v)
{
    uint8_t *in = (uint8_t *)&v;
    int cur;
    for (cur = 3; cur > 0; cur--) {
        if (in[cur] == 0) {
            bs_write_bit(bs, 1);
        } else {
            bs_write_bit(bs, 0);
            bs_write_bits(bs, in, (cur + 1) << 3, 1);
            return;
        }
    }
    if ((in[0] & 0xF0) == 0) {
        bs_write_bit(bs, 1);
        bs_write_bits(bs, in, 4, 1);
    } else {
        bs_write_bit(bs, 0);
        bs_write_bits(bs, in, 8, 1);
    }
}

static int bs_read_compressed(BitStream *bs, uint32_t *v)
{
    uint8_t *out = (uint8_t *)v;
    int cur, b;
    *v = 0;
    for (cur = 3; cur > 0; cur--) {
        if (!bs_read_bit(bs, &b))
            return 0;
        if (!b)
            return bs_read_bits(bs, out, (cur + 1) << 3, 1);
        out[cur] = 0;
    }
    if (!bs_read_bit(bs, &b))
        return 0;
    return bs_read_bits(bs, out, b ? 4 : 8, 1);
}

static BitStream *bs_copy(const uint8_t *data, int bits)
{
    BitStream *bs = bs_new();
    bs_write_bits(bs, data, bits, 0);
    return bs;
}

/* ---- hooks -------------------------------------------------------------- */

typedef struct {
    BitStream *bs;
    int id, priority, reliability, channel, shift;
} HookParams;

static struct { Script *script; int32_t label; } stubs[HOOK_TYPES];
static HookParams *current;
static int hook_result;

/* Runs MoonLoader's handler for one hook type; false means drop it. */
static int run_hook(int type, HookParams *hp)
{
    HookParams *outer = current;
    int outer_result = hook_result, ok;
    if (!stubs[type].script)
        return 1;
    current = hp;
    hook_result = 1;
    hp->bs->read_offset = 0;
    ok = script_run_label(stubs[type].script, stubs[type].label) ? hook_result != 0 : 1;
    current = outer;
    hook_result = outer_result;
    return ok;
}

typedef int (THISCALL *SendFn)(void *, BitStream *, int, int, int);
typedef Packet *(THISCALL *ReceiveFn)(void *);
typedef void (THISCALL *DeallocFn)(void *, Packet *);
typedef int (THISCALL *RpcFn)(void *, int *, BitStream *, int, int, int, int);
typedef int (THISCALL *HandleRpcFn)(void *, const uint8_t *, int, uint32_t, uint32_t);

static void **vtable;
static SendFn orig_send;
static ReceiveFn orig_receive;
static DeallocFn orig_dealloc;
static RpcFn orig_rpc;
static HandleRpcFn orig_handle_rpc;
static uint32_t server_id[2];
static int have_server_id;

static void *rak_client(void) { return samp_rakclient(); }

static int THISCALL hook_send(void *rc, BitStream *bs, int priority, int reliability, int channel)
{
    HookParams hp;
    int ok;
    if (!stubs[OUT_PACKET].script || !bs)
        return orig_send(rc, bs, priority, reliability, channel);
    hp.bs = bs_copy(bs->data, bs->bits_used);
    hp.id = bs->bits_used >= 8 ? bs->data[0] : -1;
    hp.priority = priority;
    hp.reliability = reliability;
    hp.channel = channel;
    hp.shift = 0;
    {
        BitStream *mine = hp.bs;
        ok = run_hook(OUT_PACKET, &hp);
        if (ok)
            ok = orig_send(rc, hp.bs, hp.priority, hp.reliability, hp.channel);
        bs_delete(mine);
    }
    return ok;
}

static int THISCALL hook_rpc(void *rc, int *id, BitStream *bs, int priority, int reliability,
                             int channel, int shift)
{
    HookParams hp;
    int ok, rpc;
    if (id && bs && *id == 62 && dialog_response_sent(bs->data, bs->bits_used))
        return 1;   /* a script's own dialog: answered here, not on the server */
    if (!stubs[OUT_RPC].script || !bs || !id)
        return orig_rpc(rc, id, bs, priority, reliability, channel, shift);
    hp.bs = bs_copy(bs->data, bs->bits_used);
    hp.id = *id;
    hp.priority = priority;
    hp.reliability = reliability;
    hp.channel = channel;
    hp.shift = shift;
    {
        BitStream *mine = hp.bs;
        ok = run_hook(OUT_RPC, &hp);
        if (ok) {
            rpc = hp.id;
            ok = orig_rpc(rc, &rpc, hp.bs, hp.priority, hp.reliability, hp.channel, hp.shift);
        }
        bs_delete(mine);
    }
    return ok;
}

/* Packets whose data moonsamp replaced; packets it made up, queued and then
 * handed out until SA-MP deallocates them. */
#define MAX_SWAPS 32
static struct { Packet *p; uint8_t *orig; } swaps[MAX_SWAPS];
static Packet *queue[MAX_SWAPS], *handed[MAX_SWAPS];
static int nqueue;

static int track(Packet **list, Packet *p)
{
    int i;
    for (i = 0; i < MAX_SWAPS; i++)
        if (!list[i]) {
            list[i] = p;
            return 1;
        }
    return 0;
}

static Packet *THISCALL hook_receive(void *rc)
{
    for (;;) {
        Packet *p;
        HookParams hp;
        int i;
        if (nqueue) {
            p = queue[0];
            for (i = 1; i < nqueue; i++)
                queue[i - 1] = queue[i];
            queue[--nqueue] = NULL;
            track(handed, p);
            return p;
        }
        p = orig_receive(rc);
        if (!p || !stubs[IN_PACKET].script || !p->data || !p->length)
            return p;
        hp.bs = bs_copy(p->data, (int)p->length * 8);
        hp.id = p->data[0];
        hp.priority = hp.reliability = hp.channel = hp.shift = 0;
        {
            BitStream *mine = hp.bs;
            int keep = run_hook(IN_PACKET, &hp);
            int bytes = (hp.bs->bits_used + 7) >> 3;
            if (!keep) {
                bs_delete(mine);
                orig_dealloc(rc, p);
                continue;
            }
            if (bytes > 0 && ((uint32_t)bytes != p->length || hp.id != p->data[0] ||
                              memcmp(hp.bs->data, p->data, bytes) != 0)) {
                for (i = 0; i < MAX_SWAPS && swaps[i].p; i++)
                    ;
                if (i < MAX_SWAPS) {
                    uint8_t *d = mem_alloc(bytes);
                    memcpy(d, hp.bs->data, bytes);
                    d[0] = (uint8_t)hp.id;
                    swaps[i].p = p;
                    swaps[i].orig = p->data;
                    p->data = d;
                    p->length = bytes;
                    p->bit_size = hp.bs->bits_used;
                }
            }
            bs_delete(mine);
        }
        return p;
    }
}

static void THISCALL hook_dealloc(void *rc, Packet *p)
{
    int i;
    for (i = 0; i < MAX_SWAPS; i++)
        if (handed[i] == p) {
            handed[i] = NULL;
            mem_free(p->data);
            mem_free(p);
            return;
        }
    for (i = 0; i < MAX_SWAPS; i++)
        if (swaps[i].p == p) {
            mem_free(p->data);
            p->data = swaps[i].orig;
            swaps[i].p = NULL;
            break;
        }
    orig_dealloc(rc, p);
}

/* [ID_TIMESTAMP time] ID_RPC id compressed(bits) bits */
static int THISCALL hook_handle_rpc(void *peer, const uint8_t *data, int length,
                                    uint32_t pid_lo, uint32_t pid_hi)
{
    BitStream in, out;
    HookParams hp;
    uint8_t id = 0;
    uint32_t bits = 0;
    int header, ok;
    server_id[0] = pid_lo;
    server_id[1] = pid_hi;
    have_server_id = 1;
    if (!stubs[IN_RPC].script || !data || length < 2)
        return orig_handle_rpc(peer, data, length, pid_lo, pid_hi);
    bs_init(&in);
    in.data = (uint8_t *)data;
    in.copy_data = 0;
    in.bits_used = in.bits_allocated = length * 8;
    in.read_offset = data[0] == ID_TIMESTAMP ? 8 * 6 : 8;
    header = in.read_offset;
    if (!bs_read_bits(&in, &id, 8, 1) || !bs_read_compressed(&in, &bits) ||
        (bits && in.read_offset + (int)bits > in.bits_used))
        return orig_handle_rpc(peer, data, length, pid_lo, pid_hi);
    hp.bs = bs_new();
    if (bits) {
        BitStream *tmp = hp.bs;
        uint8_t *buf = mem_alloc((bits + 7) >> 3);
        bs_read_bits(&in, buf, bits, 0);
        bs_write_bits(tmp, buf, bits, 0);
        mem_free(buf);
    }
    hp.id = id;
    hp.priority = hp.reliability = hp.channel = hp.shift = 0;
    {
        BitStream *mine = hp.bs;
        if (!run_hook(IN_RPC, &hp)) {
            bs_delete(mine);
            return 0;
        }
        bs_init(&out);
        bs_write_bits(&out, data, header, 0);
        id = (uint8_t)hp.id;
        bs_write_bits(&out, &id, 8, 1);
        bs_write_compressed(&out, (uint32_t)hp.bs->bits_used);
        bs_write_bits(&out, hp.bs->data, hp.bs->bits_used, 0);
        bs_delete(mine);
    }
    ok = orig_handle_rpc(peer, out.data, (out.bits_used + 7) >> 3, pid_lo, pid_hi);
    bs_release(&out);
    return ok;
}

static int patch_slot(int i, void *fn, void **orig)
{
    *orig = vtable[i];
    return ms_patch(&vtable[i], &fn, 4);
}

/* Called every frame; hooks RakNet once the RakClient exists. */
void raknet_tick(void)
{
    const Build *b = samp_build();
    void *rc;
    uint8_t *site;
    int32_t rel;
    if (vtable || !b || !(rc = rak_client()))
        return;
    vtable = *(void ***)rc;
    site = (uint8_t *)(samp_base + b->handle_rpc_call);
    if (site[0] != 0xE8 || site + 5 + *(int32_t *)(site + 1) != (uint8_t *)(samp_base + b->handle_rpc)) {
        ms_log("HandleRPCPacket call not found at %08lX; RakNet hooks off", (DWORD)site);
        return;
    }
    orig_handle_rpc = (HandleRpcFn)(samp_base + b->handle_rpc);
    rel = (int32_t)((uint8_t *)hook_handle_rpc - (site + 5));
    if (!patch_slot(6, (void *)hook_send, (void **)&orig_send) ||
        !patch_slot(8, (void *)hook_receive, (void **)&orig_receive) ||
        !patch_slot(9, (void *)hook_dealloc, (void **)&orig_dealloc) ||
        !patch_slot(25, (void *)hook_rpc, (void **)&orig_rpc) ||
        !ms_patch(site + 1, &rel, 4)) {
        ms_log("could not hook RakNet");
        return;
    }
    ms_log("RakNet hooked (RakClient %08lX)", (DWORD)rc);
}

/* ---- opcodes ------------------------------------------------------------ */

static BitStream *arg_bs(Script *s) { return (BitStream *)(uintptr_t)script_int(s); }

static void store_int(Script *s, int32_t v)
{
    ScriptParams[0].i = v;
    script_store(s, 1);
}

/* 0B3D raknet %1d% = new_bit_stream */
static int8_t __stdcall op_0B3D(Script *s) { store_int(s, (int32_t)(uintptr_t)bs_new()); return 0; }

/* 0B3E raknet delete_bit_stream %1d% */
static int8_t __stdcall op_0B3E(Script *s) { bs_delete(arg_bs(s)); return 0; }

/* 0B3F raknet reset_bit_stream %1d% */
static int8_t __stdcall op_0B3F(Script *s)
{
    BitStream *bs = arg_bs(s);
    if (bs)
        bs->bits_used = bs->read_offset = 0;
    return 0;
}

/* 0B40 raknet bit_stream %1d% write %2d% type %3d% size %4d% */
static int8_t __stdcall op_0B40(Script *s)
{
    BitStream *bs;
    uint32_t v;
    int type, size;
    script_collect(s, 4);
    bs = (BitStream *)ScriptParams[0].p;
    v = ScriptParams[1].u;
    type = ScriptParams[2].i;
    size = ScriptParams[3].i;
    if (!bs)
        return 0;
    switch (type) {
    case BS_BYTE:  bs_write_bytes(bs, &v, 1); break;
    case BS_BOOL:  bs_write_bit(bs, v != 0); break;
    case BS_SHORT: bs_write_bytes(bs, &v, 2); break;
    case BS_INT:
    case BS_FLOAT: bs_write_bytes(bs, &v, 4); break;
    case BS_ARRAY:
        if (v && size > 0)
            bs_write_bytes(bs, (const void *)(uintptr_t)v, size);
        break;
    case BS_BITSTREAM:
        if (v)
            bs_write_bits(bs, ((BitStream *)(uintptr_t)v)->data, ((BitStream *)(uintptr_t)v)->bits_used, 0);
        break;
    }
    return 0;
}

/* 0BE7 raknet %3d% = bit_stream_read %1d% type %2d% */
static int8_t __stdcall op_0BE7(Script *s)
{
    BitStream *bs;
    int type, b;
    uint32_t v = 0;
    script_collect(s, 2);
    bs = (BitStream *)ScriptParams[0].p;
    type = ScriptParams[1].i;
    if (bs)
        switch (type) {
        case BS_BYTE:  bs_read_bits(bs, (uint8_t *)&v, 8, 1); break;
        case BS_BOOL:  if (bs_read_bit(bs, &b)) v = b; break;
        case BS_SHORT: bs_read_bits(bs, (uint8_t *)&v, 16, 1); break;
        case BS_INT:
        case BS_FLOAT: bs_read_bits(bs, (uint8_t *)&v, 32, 1); break;
        }
    store_int(s, (int32_t)v);
    return 0;
}

/* 0BE8 raknet bit_stream %1d% read_array %2d% size %3d% */
static int8_t __stdcall op_0BE8(Script *s)
{
    BitStream *bs;
    uint8_t *dst;
    int size;
    script_collect(s, 3);
    bs = (BitStream *)ScriptParams[0].p;
    dst = (uint8_t *)ScriptParams[1].p;
    size = ScriptParams[2].i;
    if (bs && dst && size > 0)
        bs_read_bits(bs, dst, size * 8, 1);
    return 0;
}

/* 0BE9 reset_read_pointer, 0BEA reset_write_pointer */
static int8_t __stdcall op_0BE9(Script *s) { BitStream *bs = arg_bs(s); if (bs) bs->read_offset = 0; return 0; }
static int8_t __stdcall op_0BEA(Script *s) { BitStream *bs = arg_bs(s); if (bs) bs->bits_used = 0; return 0; }

/* 0BEB ignore_bits, 0BEC set_write_offset, 0BED set_read_offset */
static int8_t __stdcall op_0BEB(Script *s)
{
    BitStream *bs;
    script_collect(s, 2);
    if ((bs = ScriptParams[0].p) != NULL)
        bs->read_offset += ScriptParams[1].i;
    return 0;
}

static int8_t __stdcall op_0BEC(Script *s)
{
    BitStream *bs;
    int off;
    script_collect(s, 2);
    bs = ScriptParams[0].p;
    off = ScriptParams[1].i;
    if (bs && off >= 0) {
        if (off > bs->bits_used)
            bs_reserve(bs, off - bs->bits_used);
        bs->bits_used = off;
    }
    return 0;
}

static int8_t __stdcall op_0BED(Script *s)
{
    BitStream *bs;
    script_collect(s, 2);
    if ((bs = ScriptParams[0].p) != NULL)
        bs->read_offset = ScriptParams[1].i;
    return 0;
}

/* 0BEE..0BF3: bits used, bytes used, unread bits, write offset, read
 * offset, data pointer. */
static int8_t bs_query(Script *s, int what)
{
    BitStream *bs = arg_bs(s);
    int32_t v = 0;
    if (bs)
        switch (what) {
        case 0: v = bs->bits_used; break;
        case 1: v = (bs->bits_used + 7) >> 3; break;
        case 2: v = bs->bits_used - bs->read_offset; break;
        case 3: v = bs->bits_used; break;
        case 4: v = bs->read_offset; break;
        case 5: v = (int32_t)(uintptr_t)bs->data; break;
        }
    store_int(s, v);
    return 0;
}
static int8_t __stdcall op_0BEE(Script *s) { return bs_query(s, 0); }
static int8_t __stdcall op_0BEF(Script *s) { return bs_query(s, 1); }
static int8_t __stdcall op_0BF0(Script *s) { return bs_query(s, 2); }
static int8_t __stdcall op_0BF1(Script *s) { return bs_query(s, 3); }
static int8_t __stdcall op_0BF2(Script *s) { return bs_query(s, 4); }
static int8_t __stdcall op_0BF3(Script *s) { return bs_query(s, 5); }

typedef int (THISCALL *CodecFn)(void *, char *, int, BitStream *, int);
typedef void (THISCALL *BsCtorFn)(BitStream *);

/* 0BF4 raknet bit_stream %1d% decode_string %2d% size %3d% */
static int8_t __stdcall op_0BF4(Script *s)
{
    const Build *b = samp_build();
    BitStream *bs;
    char *buf;
    int size;
    script_collect(s, 3);
    bs = ScriptParams[0].p;
    buf = ScriptParams[1].p;
    size = ScriptParams[2].i;
    if (buf && size > 0)
        buf[0] = 0;
    if (b && bs && buf && size > 0) {
        void *sc = *(void **)(samp_base + b->string_compressor);
        if (sc)
            ((CodecFn)(samp_base + b->decode_string))(sc, buf, size, bs, 0);
    }
    return 0;
}

/* 0BF5 raknet bit_stream %1d% encode_string %2d% size %3d% */
static int8_t __stdcall op_0BF5(Script *s)
{
    const Build *b = samp_build();
    BitStream *bs, tmp;
    char *str;
    int size;
    void *sc;
    script_collect(s, 3);
    bs = ScriptParams[0].p;
    str = ScriptParams[1].p;
    size = ScriptParams[2].i;
    if (!b || !bs || !str || size < 0 || !(sc = *(void **)(samp_base + b->string_compressor)))
        return 0;
    /* samp.dll may grow the stream: give it one of its own. */
    ((BsCtorFn)(samp_base + b->bs_ctor))(&tmp);
    ((CodecFn)(samp_base + b->encode_string))(sc, str, size, &tmp, 0);
    bs_write_bits(bs, tmp.data, tmp.bits_used, 0);
    ((BsCtorFn)(samp_base + b->bs_dtor))(&tmp);
    return 0;
}

/* 0BE0 raknet hook_ret %1d% */
static int8_t __stdcall op_0BE0(Script *s)
{
    hook_result = script_int(s);
    script_label_return();
    return 0;
}

/* 0BE1..0BE4 raknet setup_*_hook %1p% */
static int8_t setup_hook(Script *s, int type)
{
    stubs[type].label = script_int(s);
    stubs[type].script = s;
    raknet_tick();
    script_cond(s, 1);
    return 0;
}
static int8_t __stdcall op_0BE1(Script *s) { return setup_hook(s, OUT_RPC); }
static int8_t __stdcall op_0BE2(Script *s) { return setup_hook(s, OUT_PACKET); }
static int8_t __stdcall op_0BE3(Script *s) { return setup_hook(s, IN_RPC); }
static int8_t __stdcall op_0BE4(Script *s) { return setup_hook(s, IN_PACKET); }

/* 0BE5 raknet %2d% = get_hook_param %1d% */
static int8_t __stdcall op_0BE5(Script *s)
{
    int i = script_int(s);
    int32_t v = 0;
    if (current)
        switch (i) {
        case 0: v = (int32_t)(uintptr_t)current->bs; break;
        case 1: v = current->id; break;
        case 2: v = current->priority; break;
        case 3: v = current->reliability; break;
        case 4: v = current->channel; break;
        case 5: v = current->shift; break;
        }
    store_int(s, v);
    return 0;
}

/* 0BE6 raknet set_hook_param %1d% = %2d% */
static int8_t __stdcall op_0BE6(Script *s)
{
    int i, v;
    script_collect(s, 2);
    i = ScriptParams[0].i;
    v = ScriptParams[1].i;
    if (current)
        switch (i) {
        case 0: if (v) current->bs = (BitStream *)(uintptr_t)v; break;
        case 1: current->id = v; break;
        case 2: current->priority = v; break;
        case 3: current->reliability = v; break;
        case 4: current->channel = v; break;
        case 5: current->shift = v; break;
        }
    return 0;
}

static int send_rpc(int id, BitStream *bs, int priority, int reliability, int channel, int shift)
{
    void *rc = rak_client();
    if (!rc || !bs)
        return 0;
    return ((RpcFn)(*(void ***)rc)[25])(rc, &id, bs, priority, reliability, channel, shift);
}

static int send_packet(BitStream *bs, int priority, int reliability, int channel)
{
    void *rc = rak_client();
    if (!rc || !bs)
        return 0;
    return ((SendFn)(*(void ***)rc)[6])(rc, bs, priority, reliability, channel);
}

/* Sends bits of data MSB first, as an RPC (rpc >= 0, HIGH_PRIORITY,
 * RELIABLE_ORDERED) or a packet (HIGH_PRIORITY, the given reliability). It
 * goes through the hooks, as SA-MP's own does. */
int raknet_send(int rpc, const uint8_t *data, int bits, int reliability)
{
    BitStream bs;
    int ok;
    bs_init(&bs);
    bs_write_bits(&bs, data, bits, 0);
    ok = rpc >= 0 ? send_rpc(rpc, &bs, 1, 9, 0, 0) : send_packet(&bs, 1, reliability, 0);
    bs_release(&bs);
    return ok;
}

/* 0B41 raknet send_rpc %1d% bit_stream %2d% priority %3d% reliability %4d% ordering_channel %5d% shift_timestamp %6d% */
static int8_t __stdcall op_0B41(Script *s)
{
    script_collect(s, 6);
    script_cond(s, send_rpc(ScriptParams[0].i, ScriptParams[1].p, ScriptParams[2].i,
                            ScriptParams[3].i, ScriptParams[4].i, ScriptParams[5].i));
    return 0;
}

/* 0B42 raknet send bit_stream %1d% priority %2d% reliability %3d% ordering_channel %4d% */
static int8_t __stdcall op_0B42(Script *s)
{
    script_collect(s, 4);
    script_cond(s, send_packet(ScriptParams[0].p, ScriptParams[1].i, ScriptParams[2].i, ScriptParams[3].i));
    return 0;
}

/* 0B8A raknet send_rpc %1d% bit_stream %2d%: HIGH_PRIORITY, RELIABLE_ORDERED, 0 */
static int8_t __stdcall op_0B8A(Script *s)
{
    script_collect(s, 2);
    script_cond(s, send_rpc(ScriptParams[0].i, ScriptParams[1].p, 1, 9, 0, 0));
    return 0;
}

/* 0B8B raknet send_bit_stream %1d%: HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0 */
static int8_t __stdcall op_0B8B(Script *s)
{
    script_cond(s, send_packet(arg_bs(s), 1, 7, 0));
    return 0;
}

/* 0BF6 raknet emul_rpc_receive %1d% bit_stream %2d%: straight to SA-MP's
 * handler, past the hooks, as from the server. */
static int8_t __stdcall op_0BF6(Script *s)
{
    BitStream *bs, out;
    uint8_t head[2];
    void *peer;
    script_collect(s, 2);
    bs = ScriptParams[1].p;
    head[0] = ID_RPC;
    head[1] = (uint8_t)ScriptParams[0].i;
    if (!bs || !orig_handle_rpc || !have_server_id || !(peer = samp_rakpeer()))
        return 0;
    bs_init(&out);
    bs_write_bytes(&out, head, 2);
    bs_write_compressed(&out, (uint32_t)bs->bits_used);
    bs_write_bits(&out, bs->data, bs->bits_used, 0);
    orig_handle_rpc(peer, out.data, (out.bits_used + 7) >> 3, server_id[0], server_id[1]);
    bs_release(&out);
    return 0;
}

/* 0BF7 raknet emul_packet_receive %1d% bit_stream %2d%: queued for SA-MP's
 * next Receive, the id byte first. */
static int8_t __stdcall op_0BF7(Script *s)
{
    BitStream *bs;
    Packet *p;
    int bytes;
    script_collect(s, 2);
    bs = ScriptParams[1].p;
    if (!bs || !vtable || nqueue >= MAX_SWAPS)
        return 0;
    bytes = 1 + ((bs->bits_used + 7) >> 3);
    p = mem_alloc(sizeof *p);
    p->data = mem_alloc(bytes);
    p->data[0] = (uint8_t)ScriptParams[0].i;
    memcpy(p->data + 1, bs->data, bytes - 1);
    p->length = bytes;
    p->bit_size = 8 + bs->bits_used;
    p->player_addr = server_id[0];
    p->player_port = (uint16_t)server_id[1];
    p->player_index = 0xFFFF;
    queue[nqueue++] = p;
    return 0;
}

/* 0BF8 raknet %2d% = get_rpc_name %1d%, 0BF9 ... get_packet_name */
static int8_t name_of(Script *s, const char *const *table)
{
    int id = script_int(s);
    ScriptParams[0].p = (void *)(id >= 0 && id < 256 && table[id] ? table[id] : "");
    script_store(s, 1);
    return 0;
}
static int8_t __stdcall op_0BF8(Script *s) { return name_of(s, rpc_names); }
static int8_t __stdcall op_0BF9(Script *s) { return name_of(s, packet_names); }

void raknet_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B3D, op_0B3D }, { 0x0B3E, op_0B3E }, { 0x0B3F, op_0B3F }, { 0x0B40, op_0B40 },
        { 0x0B41, op_0B41 }, { 0x0B42, op_0B42 }, { 0x0B8A, op_0B8A }, { 0x0B8B, op_0B8B },
        { 0x0BE0, op_0BE0 }, { 0x0BE1, op_0BE1 }, { 0x0BE2, op_0BE2 }, { 0x0BE3, op_0BE3 },
        { 0x0BE4, op_0BE4 }, { 0x0BE5, op_0BE5 }, { 0x0BE6, op_0BE6 }, { 0x0BE7, op_0BE7 },
        { 0x0BE8, op_0BE8 }, { 0x0BE9, op_0BE9 }, { 0x0BEA, op_0BEA }, { 0x0BEB, op_0BEB },
        { 0x0BEC, op_0BEC }, { 0x0BED, op_0BED }, { 0x0BEE, op_0BEE }, { 0x0BEF, op_0BEF },
        { 0x0BF0, op_0BF0 }, { 0x0BF1, op_0BF1 }, { 0x0BF2, op_0BF2 }, { 0x0BF3, op_0BF3 },
        { 0x0BF4, op_0BF4 }, { 0x0BF5, op_0BF5 }, { 0x0BF6, op_0BF6 }, { 0x0BF7, op_0BF7 },
        { 0x0BF8, op_0BF8 }, { 0x0BF9, op_0BF9 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
