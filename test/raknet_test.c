/* moonsamp's RakNet side under Wine (see run.sh). samp.dll is mapped without
 * running it; its CNetGame pointer is set to a fake whose RakClient vtable
 * belongs to this test, and its HandleRPCPacket jumps here. MoonLoader is
 * emulated byte for byte: hook stubs `0AA5 fn; 0BE0 local10`, and the
 * opcodes its handler calls run on the hook script with base/IP swapped to
 * a parameter buffer, as MoonLoader's 0x101274B0 does. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../src/builds.h"

typedef int8_t (THISCALL *Handler)(void *, int);

typedef struct {
    int bits_used, bits_allocated, read_offset;
    uint8_t *data;
    uint8_t copy_data, stack_data[256];
} __attribute__((packed)) BitStream;

typedef struct {
    uint16_t player_index;
    uint32_t player_addr;
    uint16_t player_port;
    uint32_t length, bit_size;
    uint8_t *data;
    uint8_t delete_data;
} __attribute__((packed)) Packet;

static int failed, passed;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (ok) passed++; else failed++;
}

/* ---- MoonLoader emulation --------------------------------------------- */

static uint8_t main_script[0x100], hook_script[0x100];
static uint8_t hook_code[64], opbuf[256];
#define BASE(s)     (*(uint8_t **)((s) + 0x10))
#define IP(s)       (*(uint8_t **)((s) + 0x14))
#define LOCAL(s, i) (*(int32_t *)((s) + 0x3C + 4 * (i)))
#define COND(s)     ((s)[0xC5])

static size_t n;
static void begin(void) { n = 0; }
static void put(const void *p, size_t len) { memcpy(opbuf + n, p, len); n += len; }
static void put_int(int32_t v) { opbuf[n++] = 0x01; put(&v, 4); }
static void put_local(int i) { uint16_t x = (uint16_t)i; opbuf[n++] = 0x03; put(&x, 2); }

static int8_t op(uint8_t *s, int code)
{
    Handler *table = *(Handler **)0x469FEE;
    uint8_t *base = BASE(s), *ip = IP(s);
    int8_t r;
    BASE(s) = IP(s) = opbuf;
    s[0xD2] = 0;
    r = table[code / 100](s, code);
    BASE(s) = base;
    IP(s) = ip;
    return r;
}

static int32_t op_out(uint8_t *s, int code)
{
    put_local(0);
    op(s, code);
    return LOCAL(s, 0);
}

static int32_t bs_new(void) { begin(); return op_out(main_script, 0x0B3D); }
static void bs_write(int32_t bs, uint32_t v, int type, int size)
{
    begin(); put_int(bs); put_int((int32_t)v); put_int(type); put_int(size);
    op(main_script, 0x0B40);
}
static int32_t bs_read(uint8_t *s, int32_t bs, int type)
{
    begin(); put_int(bs); put_int(type); return op_out(s, 0x0BE7);
}
static int32_t bs_q(uint8_t *s, int code, int32_t bs) { begin(); put_int(bs); return op_out(s, code); }
static void bs_2(uint8_t *s, int code, int32_t bs, int32_t v) { begin(); put_int(bs); put_int(v); op(s, code); }
static int32_t hook_param(int i) { begin(); put_int(i); return op_out(hook_script, 0x0BE5); }
static void set_hook_param(int i, int32_t v) { begin(); put_int(i); put_int(v); op(hook_script, 0x0BE6); }

/* What the Lua side does, per test. */
static int mode, calls;
static int32_t seen[6];
static int32_t seen_value;

static void __cdecl ml_handler(void)
{
    int i;
    calls++;
    for (i = 0; i < 6; i++)
        seen[i] = hook_param(i);
    LOCAL(hook_script, 10) = 1;
    switch (mode) {
    case 1:     /* outgoing RPC: read, raise the priority */
        seen_value = bs_read(hook_script, seen[0], 3);
        set_hook_param(2, 2);
        break;
    case 2:     /* drop it */
        LOCAL(hook_script, 10) = 0;
        break;
    case 3:     /* incoming RPC: read an int16, rewrite as one byte, new id */
        seen_value = bs_read(hook_script, seen[0], 2);
        bs_q(hook_script, 0x0BEA, seen[0]);
        begin(); put_int(seen[0]); put_int(0xAB); put_int(0); put_int(0);
        op(hook_script, 0x0B40);
        set_hook_param(1, 0x60);
        break;
    case 4:     /* incoming packet: overwrite the second byte, cut the rest */
        bs_2(hook_script, 0x0BEC, seen[0], 8);
        begin(); put_int(seen[0]); put_int(0x7F); put_int(0); put_int(0);
        op(hook_script, 0x0B40);
        break;
    }
}

static void setup_hook(int code)
{
    begin();
    put_int(-16);
    op(hook_script, code);
}

/* ---- fake samp.dll ------------------------------------------------------ */

static struct { int called, id, prio, rel, chan, shift, bits; uint8_t data[64]; } last_rpc, last_send;
static struct { int called, len; uint8_t data[64]; uint32_t pid[2]; } last_handle;
static Packet *queue[4];
static int nqueue;
static uint8_t *dealloc_data[4];
static int ndealloc;

static int THISCALL f_send(void *rc, BitStream *bs, int prio, int rel, int chan)
{
    (void)rc;
    last_send.called++;
    last_send.prio = prio; last_send.rel = rel; last_send.chan = chan;
    last_send.bits = bs->bits_used;
    memcpy(last_send.data, bs->data, (bs->bits_used + 7) / 8);
    return 1;
}

static int THISCALL f_rpc(void *rc, int *id, BitStream *bs, int prio, int rel, int chan, int shift)
{
    (void)rc;
    last_rpc.called++;
    last_rpc.id = *id; last_rpc.prio = prio; last_rpc.rel = rel;
    last_rpc.chan = chan; last_rpc.shift = shift;
    last_rpc.bits = bs->bits_used;
    memcpy(last_rpc.data, bs->data, (bs->bits_used + 7) / 8);
    return 1;
}

static Packet *THISCALL f_receive(void *rc)
{
    Packet *p;
    int i;
    (void)rc;
    if (!nqueue)
        return NULL;
    p = queue[0];
    for (i = 1; i < nqueue; i++)
        queue[i - 1] = queue[i];
    nqueue--;
    return p;
}

static void THISCALL f_dealloc(void *rc, Packet *p)
{
    (void)rc;
    dealloc_data[ndealloc++ & 3] = p->data;
}

static int THISCALL f_handle(void *peer, const uint8_t *data, int len, uint32_t lo, uint32_t hi)
{
    (void)peer;
    last_handle.called++;
    last_handle.len = len;
    memcpy(last_handle.data, data, len < 64 ? len : 64);
    last_handle.pid[0] = lo;
    last_handle.pid[1] = hi;
    return 1;
}

static void *vtable[40];
static struct { void **vt; } rak_client = { vtable };
static uint8_t netgame[0x400];

static Packet *packet(const uint8_t *d, int len)
{
    static Packet pk[4];
    static uint8_t buf[4][16];
    static int k;
    Packet *p = &pk[k];
    memcpy(buf[k], d, len);
    p->data = buf[k];
    p->length = len;
    p->bit_size = len * 8;
    k = (k + 1) & 3;
    return p;
}

#define CALL_RPC(rc, id, bs, a, b, c, d) \
    ((int (THISCALL *)(void *, int *, BitStream *, int, int, int, int))vtable[25])(rc, id, bs, a, b, c, d)

__declspec(dllexport) int run(void)
{
    HMODULE samp, ms;
    uintptr_t base;
    uint32_t entry;
    const Build *L = NULL;
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    samp = LoadLibraryExA("samp.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
    check(samp != NULL, "samp.dll maps without running");
    if (!samp)
        return 1;
    base = (uintptr_t)samp;
    entry = *(uint32_t *)(base + *(uint32_t *)(base + 0x3C) + 0x28);
    for (i = 0; i < (int)(sizeof builds / sizeof builds[0]); i++)
        if (builds[i].entry == entry)
            L = &builds[i];
    if (!L) { printf("samp.dll build %x not covered\n", entry); return 1; }
    printf("build %s\n", L->name);

    /* HandleRPCPacket -> f_handle; CNetGame -> fake holding the RakClient */
    {
        uint8_t jmp[5] = { 0xE9 };
        DWORD old;
        uint8_t *h = (uint8_t *)(base + L->handle_rpc);
        *(int32_t *)(jmp + 1) = (int32_t)((uint8_t *)f_handle - (h + 5));
        VirtualProtect(h, 5, PAGE_EXECUTE_READWRITE, &old);
        memcpy(h, jmp, 5);
        *(void **)(netgame + L->ng_rakclient) = &rak_client;
        VirtualProtect((void *)(base + L->netgame), 4, PAGE_READWRITE, &old);
        *(void **)(base + L->netgame) = netgame;
    }
    vtable[6] = (void *)f_send;
    vtable[8] = (void *)f_receive;
    vtable[9] = (void *)f_dealloc;
    vtable[25] = (void *)f_rpc;

    ms = LoadLibraryA("moonsamp.asi");
    check(ms != NULL, "moonsamp.asi loads");
    if (!ms)
        return 1;

    /* the hook script: its code is MoonLoader's stub at base + 16 */
    {
        uint8_t stub[] = { 0xA5, 0x0A, 0x01, 0, 0, 0, 0, 0x04, 0x00, 0x04, 0x00, 0x00,
                           0xE0, 0x0B, 0x03, 0x0A, 0x00 };
        *(uint32_t *)(stub + 3) = (uint32_t)(uintptr_t)ml_handler;
        memcpy(hook_code + 16, stub, sizeof stub);
        BASE(hook_script) = IP(hook_script) = hook_code;
    }

    /* BitStream opcodes */
    {
        int32_t bs = bs_new();
        BitStream *b = (BitStream *)(uintptr_t)bs;
        static const uint8_t arr[3] = { 1, 2, 3 };
        float f = 1.5f;
        check(bs != 0 && b->bits_allocated == 2048 && b->data == b->stack_data,
              "0B3D makes an empty RakNet BitStream");
        bs_write(bs, 1, 1, 0);
        bs_write(bs, 0xAB, 0, 0);
        check(b->bits_used == 9 && b->data[0] == 0xD5 && b->data[1] == 0x80,
              "bool 1 + int8 AB pack as D5 80 (MSB first, RakNet)");
        bs_write(bs, 0xBEEF, 2, 0);
        bs_write(bs, 0x11223344, 3, 0);
        bs_write(bs, *(uint32_t *)&f, 4, 0);
        bs_write(bs, (uint32_t)(uintptr_t)arr, 5, 3);
        check(bs_q(main_script, 0x0BEE, bs) == 9 + 16 + 32 + 32 + 24, "0BEE bits used");
        check(bs_q(main_script, 0x0BEF, bs) == 15, "0BEF bytes used");
        check(bs_read(main_script, bs, 1) == 1 && bs_read(main_script, bs, 0) == 0xAB,
              "reads back bool and int8");
        check(bs_read(main_script, bs, 2) == 0xBEEF, "int16 reads unsigned (0xBEEF)");
        check(bs_read(main_script, bs, 3) == 0x11223344, "int32 reads back");
        {
            int32_t v = bs_read(main_script, bs, 4);
            check(*(float *)&v == 1.5f, "float reads back raw");
        }
        {
            uint8_t out[3] = { 0 };
            begin(); put_int(bs); put_int((int32_t)(uintptr_t)out); put_int(3);
            op(main_script, 0x0BE8);
            check(memcmp(out, arr, 3) == 0, "0BE8 reads an array at a 1-bit offset");
        }
        check(bs_q(main_script, 0x0BF0, bs) == 0, "0BF0 nothing left unread");
        bs_2(main_script, 0x0BED, bs, 1);
        check(bs_read(main_script, bs, 0) == 0xAB, "0BED sets the read offset");
        bs_q(main_script, 0x0BE9, bs);
        bs_2(main_script, 0x0BEB, bs, 9);
        check(bs_q(main_script, 0x0BF2, bs) == 9, "0BE9 resets, 0BEB skips bits");
        for (i = 0; i < 300; i++)
            bs_write(bs, i, 3, 0);
        check(b->bits_used == 113 + 300 * 32 && b->data != b->stack_data,
              "grows past the 256-byte stack buffer");
        bs_q(main_script, 0x0BE9, bs);
        bs_2(main_script, 0x0BEB, bs, 113 + 299 * 32);
        check(bs_read(main_script, bs, 3) == 299, "data survives the move to the heap");
        begin(); put_int(bs); op(main_script, 0x0B3E);
    }

    /* outgoing RPC hook */
    setup_hook(0x0BE1);
    check(COND(hook_script) == 1, "0BE1 sets up the outgoing RPC hook");
    check(vtable[25] != (void *)f_rpc && vtable[6] != (void *)f_send, "RakClient vtable hooked");
    {
        int32_t bs = bs_new();
        int id = 101;
        bs_write(bs, 0x11223344, 3, 0);
        bs_write(bs, 1, 1, 0);
        mode = 1; calls = 0;
        CALL_RPC(&rak_client, &id, (BitStream *)(uintptr_t)bs, 1, 9, 0, 0);
        check(calls == 1 && seen[1] == 101 && seen[2] == 1 && seen[3] == 9 && seen[4] == 0,
              "handler gets id, priority, reliability, channel from 0BE5");
        check(seen[0] != bs && seen_value == 0x11223344, "handler reads a copy of the stream");
        check(last_rpc.called == 1 && last_rpc.id == 101 && last_rpc.prio == 2 && last_rpc.bits == 33 &&
              memcmp(last_rpc.data, "\x44\x33\x22\x11\x80", 5) == 0,
              "SA-MP sends it with the priority 0BE6 set");
        mode = 2;
        check(CALL_RPC(&rak_client, &id, (BitStream *)(uintptr_t)bs, 1, 9, 0, 0) == 0 && last_rpc.called == 1,
              "hook_ret 0 drops the RPC");
        mode = 0;
        begin(); put_int(55); put_int(bs); op(main_script, 0x0B8A);
        check(COND(main_script) == 1 && last_rpc.called == 2 && last_rpc.id == 55 &&
              last_rpc.prio == 1 && last_rpc.rel == 9, "0B8A sends HIGH_PRIORITY RELIABLE_ORDERED");
        begin(); put_int(bs); op(main_script, 0x0B8B);
        check(last_send.called == 1 && last_send.rel == 7 && last_send.bits == 33,
              "0B8B sends UNRELIABLE_SEQUENCED");
    }

    /* incoming RPC: 14 5D | 111 0 00010000 | 12 34 */
    setup_hook(0x0BE3);
    {
        static const uint8_t in[] = { 0x14, 0x5D, 0xE1, 0x01, 0x23, 0x40 };
        uint8_t *site;
        int (THISCALL *handle)(void *, const uint8_t *, int, uint32_t, uint32_t);
        site = (uint8_t *)(base + L->handle_rpc_call);
        handle = (void *)(site + 5 + *(int32_t *)(site + 1));
        check((uintptr_t)handle != base + L->handle_rpc, "HandleRPCPacket call site hooked");
        mode = 3; calls = 0;
        handle(&rak_client, in, sizeof in, 0x0100007F, 7777);
        check(calls == 1 && seen[1] == 0x5D && seen_value == 0x3412,
              "parses id 0x5D and a 16-bit payload (compressed length)");
        check(last_handle.called == 1 && last_handle.len == 4 &&
              memcmp(last_handle.data, "\x14\x60\xF8\xAB", 4) == 0,
              "rebuilds 14 60 F8 AB: new id, 8-bit compressed length, AB");
        mode = 2;
        handle(&rak_client, in, sizeof in, 0x0100007F, 7777);
        check(last_handle.called == 1, "hook_ret 0 drops the RPC");
        mode = 0;
        {
            int32_t bs = bs_new();
            bs_write(bs, 0xFF, 0, 0);
            begin(); put_int(0x22); put_int(bs); op(main_script, 0x0BF6);
            check(last_handle.called == 2 && last_handle.len == 4 &&
                  memcmp(last_handle.data, "\x14\x22\xF8\xFF", 4) == 0 &&
                  last_handle.pid[0] == 0x0100007F && last_handle.pid[1] == 7777,
                  "0BF6 hands SA-MP 14 22 F8 FF from the server's id, past the hook");
        }
    }

    /* incoming packets */
    setup_hook(0x0BE4);
    {
        Packet *(THISCALL *receive)(void *) = (void *)vtable[8];
        void (THISCALL *dealloc)(void *, Packet *) = (void *)vtable[9];
        Packet *p, *q;
        uint8_t *orig;
        queue[nqueue++] = p = packet((const uint8_t *)"\xCF\x01\x02", 3);
        orig = p->data;
        mode = 4; calls = 0;
        q = receive(&rak_client);
        check(q == p && calls == 1 && seen[1] == 0xCF, "packet hook sees id 0xCF");
        check(q->length == 2 && q->data[0] == 0xCF && q->data[1] == 0x7F && q->data != orig,
              "SA-MP gets the rewritten packet CF 7F");
        dealloc(&rak_client, q);
        check(ndealloc == 1 && dealloc_data[0] == orig, "DeallocatePacket gets RakNet's own buffer back");
        queue[nqueue++] = packet((const uint8_t *)"\xCF\x09", 2);
        mode = 2;
        q = receive(&rak_client);
        check(q == NULL && ndealloc == 2, "a dropped packet is deallocated and skipped");
        mode = 0; calls = 0;
        {
            int32_t bs = bs_new();
            bs_write(bs, 0x01, 0, 0);
            begin(); put_int(0xC8); put_int(bs); op(main_script, 0x0BF7);
            q = receive(&rak_client);
            check(q && q->length == 2 && q->data[0] == 0xC8 && q->data[1] == 0x01 && calls == 0,
                  "0BF7 queues C8 01 for SA-MP's next Receive");
            dealloc(&rak_client, q);
            check(ndealloc == 2, "moonsamp frees its own packet");
        }
    }

    printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
