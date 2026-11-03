/* moonsamp's phase 2 opcodes (players, dialogs, textdraws, server) under
 * Wine (see run.sh). samp.dll is mapped without running; its globals point at
 * fake objects laid out as the build lays them out, and every samp.dll
 * function the opcodes call jumps to a stub here that records its arguments.
 * Parameters are written the way MoonLoader writes them. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define THISCALL __attribute__((thiscall))
typedef int8_t (THISCALL *Handler)(void *, int);

static int failed, passed;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (ok) passed++; else failed++;
}

/* ---- MoonLoader emulation (see raknet_test.c) ---------------------------- */

static uint8_t script[0x100], opbuf[512];
#define LOCAL(i) (*(int32_t *)(script + 0x3C + 4 * (i)))
#define LOCALF(i) (*(float *)(script + 0x3C + 4 * (i)))
#define COND     (script[0xC5])
static size_t n;
static void begin(void) { n = 0; }
static void put(const void *p, size_t len) { memcpy(opbuf + n, p, len); n += len; }
static void put_int(int32_t v) { opbuf[n++] = 0x01; put(&v, 4); }
static void put_i8(int8_t v) { opbuf[n++] = 0x04; opbuf[n++] = (uint8_t)v; }
static void put_float(float v) { opbuf[n++] = 0x06; put(&v, 4); }
static void put_local(int i) { uint16_t x = (uint16_t)i; opbuf[n++] = 0x03; put(&x, 2); }
static void put_str(const char *s)
{
    size_t len = strlen(s);
    opbuf[n++] = 0x0E;
    opbuf[n++] = (uint8_t)len;
    put(s, len);
}
static void op(int code)
{
    Handler *table = *(Handler **)0x469FEE;
    *(uint8_t **)(script + 0x10) = *(uint8_t **)(script + 0x14) = opbuf;
    script[0xD2] = 0;
    COND = 0xAA;
    table[code / 100](script, code);
}
static int32_t op_int(int code, int32_t arg) { begin(); put_int(arg); put_local(0); op(code); return LOCAL(0); }
static int op_cond(int code, int32_t arg) { begin(); put_int(arg); op(code); return COND; }
static int32_t op_out(int code) { begin(); put_local(0); op(code); return LOCAL(0); }
static int op_cond0(int code) { begin(); op(code); return COND; }

/* ---- the build ------------------------------------------------------------ */

typedef struct {
    uint32_t entry, netgame, chat, input, dialog, game, scoreboard;
    uint16_t pp_largest, pp_used, pp_info, pp_local_id, pp_local_name, pp_local_obj, pp_local_ping;
    uint8_t info_remote, info_name, info_ping, remote_ped;
    uint32_t show, close, lb_sel, lb_select, eb_get, eb_set, shutdown, cursor, setname, special, td_set;
    uint32_t vehicle_find, player_color, add_message, current_anim, anim_entry;
    uint16_t remote_anim, local_score;
    uint8_t info_score, info_npc;
} Layout;

static const Layout layouts[] = {
    { 0xCC4D0, 0x26E8DC, 0x26E8C8, 0x26E8CC, 0x26E898, 0x26E8F4, 0x26E894,
      0x0000, 0x0FB4, 0x0004, 0x2F1C, 0x2F22, 0x2F3A, 0x2F14, 0x00, 0x0C, 0x04, 0x00,
      0x6F8C0, 0x6FF40, 0x88760, 0x8A2E0, 0x84F40, 0x84E70, 0xA1E0, 0x9FFE0, 0xB5C0, 0x3340, 0xB26D0, 0x1E440, 0xA6AF0, 0x679F0, 0xADF00, 0x9C7C0, 0x105, 0x2F18, 0x24, 0x28 },
    { 0xFDB60, 0x2ACA24, 0x2ACA10, 0x2ACA14, 0x2AC9E0, 0x2ACA3C, 0x2AC9DC,
      0x0022, 0x0FD6, 0x0026, 0x0000, 0x0006, 0x001E, 0x2F36, 0x08, 0x14, 0x0C, 0x04,
      0x6FA50, 0x700D0, 0x888F0, 0x8A470, 0x850D0, 0x85000, 0xA230, 0xA0530, 0xB5E0, 0x3390, 0xB2B60, 0x1E650, 0xA6F70, 0x67BE0, 0xAE390, 0x9C950, 0x07C, 0x2F3A, 0x00, 0x04 },
};

#define AT(p, off, T) (*(T *)((uint8_t *)(p) + (off)))

/* ---- stubs for samp.dll's functions ---------------------------------------- */

static struct { int called; void *self; int a, b; char s[5][256]; int c; } last;
static void rec(void *self, int a, int b) { last.called++; last.self = self; last.a = a; last.b = b; }

static void THISCALL s_show(void *d, int id, int style, const char *cap, const char *text,
                            const char *b1, const char *b2, int serverside)
{
    rec(d, id, style);
    strcpy(last.s[0], cap); strcpy(last.s[1], text); strcpy(last.s[2], b1); strcpy(last.s[3], b2);
    last.c = serverside;
    AT(d, 0x81, int) = serverside;
}
static void THISCALL s_close(void *d, char button) { rec(d, button, 0); }
static int THISCALL s_lb_sel(void *lb, int prev) { rec(lb, prev, 0); return 5; }
static void THISCALL s_lb_select(void *lb, int i) { rec(lb, i, 0); }
static const char *THISCALL s_eb_get(void *eb) { rec(eb, 0, 0); return "typed"; }
static void THISCALL s_eb_set(void *eb, const char *t, int sel) { rec(eb, sel, 0); strcpy(last.s[0], t); }
static void THISCALL s_shutdown(void *ng) { rec(ng, 0, 0); }
static void THISCALL s_cursor(void *g, int mode, int hide) { rec(g, mode, hide); }
static void THISCALL s_setname(void *pp, const char *name) { rec(pp, 0, 0); strcpy(last.s[0], name); }
static char THISCALL s_special(void *lp) { rec(lp, 0, 0); return 7; }
static void THISCALL s_td_set(void *td, const char *t) { rec(td, 0, 0); strcpy(last.s[0], t); }
static void *anim_ped;
static uint32_t THISCALL s_current_anim(void *ped) { anim_ped = ped; return 0x00041189 & 0xFFFF0000 | 1189; }
static const char *__cdecl s_anim_entry(int id) { return id == 1189 ? "PED:IDLE_STANCE" : id == 7 ? "BAR:BARMAN_IDLE" : NULL; }
static uint32_t chat_color;
static char chat_text[160];
static void THISCALL s_add_message(void *c, uint32_t color, const char *text)
{
    rec(c, 0, 0);
    chat_color = color;
    strcpy(chat_text, text);
}
static uint32_t __stdcall s_color(int id) { return id == 1 ? 0x11223300 : 0xAABBCCFF; }
static uint8_t vehicles[3 * 0xA18];
static uint16_t THISCALL s_vehicle_find(void *vp, void *car) { rec(vp, 0, 0); return car == vehicles + 0xA18 ? 12 : 0xFFFF; }

/* GTA's CPool: objects, flag bytes, size; a handle is index << 8 | flag. */
static uint8_t peds[3 * 0x7C4], ped_flags[3] = { 0x80, 0x81, 0x05 }, vehicle_flags[3] = { 0x80, 0x07, 0x80 };
static struct { void *objects; uint8_t *flags; int size, first_free; } ped_pool = { peds, ped_flags, 3, 0 },
    vehicle_pool = { vehicles, vehicle_flags, 3, 0 };

/* The x87 register stack, empty when no slot is in use. */
static unsigned x87_tags(void)
{
    uint8_t env[28];
    __asm__ volatile("fnstenv %0\n\tfldenv %0" : "=m"(env));
    return *(uint16_t *)(env + 8);
}

static int rpc_calls, rpc_id;
static uint8_t rpc_data[64];
static int THISCALL f_rpc(void *rc, int *id, uint8_t *bs, int p, int r, int c, int sh)
{
    (void)rc; (void)p; (void)r; (void)c; (void)sh;
    rpc_calls++;
    rpc_id = *id;
    memcpy(rpc_data, *(uint8_t **)(bs + 0xC), (*(int *)bs + 7) / 8);
    return 1;
}
static int disc_calls, disc_block;
static void THISCALL f_disconnect(void *rc, unsigned block, unsigned char ch) { (void)rc; (void)ch; disc_calls++; disc_block = block; }
static int THISCALL f_nop(void *rc) { (void)rc; return 0; }

static void *vtable[40];
static struct { void **vt; } rak = { vtable };

static void jump(uintptr_t base, uint32_t rva, void *to)
{
    uint8_t *p = (uint8_t *)(base + rva), j[5] = { 0xE9 };
    DWORD old;
    *(int32_t *)(j + 1) = (int32_t)((uint8_t *)to - (p + 5));
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(p, j, 5);
}

static void set_global(uintptr_t base, uint32_t rva, void *v)
{
    DWORD old;
    VirtualProtect((void *)(base + rva), 4, PAGE_READWRITE, &old);
    *(void **)(base + rva) = v;
}

/* MSVC std::string as samp.dll holds it: buffer, then size and capacity. */
static void set_name(uint8_t *p, const char *s)
{
    strcpy((char *)p, s);
    AT(p, 16, int) = (int)strlen(s);
    AT(p, 20, int) = 15;
}

static uint8_t netgame[0x400], pools[0x40], pp[0x3000], lp[0x400], info1[0x40], info4[0x40];
static uint8_t remote1[0x200], remote4[0x200], label_pool[0xE800 + 0x2000];
static uint8_t td_pool[0x2400 + 2304 * 4], td[0x9D6], gz_pool[16], dlg[0x100], game[0x100];
static uint8_t scoreboard[0x44], chat[0x63EA], input[0x1B00], listbox[16], editbox[16];

__declspec(dllexport) int run(void)
{
    HMODULE samp;
    uintptr_t base;
    const Layout *L = NULL;
    uint32_t entry;
    size_t i;
    char buf[0x1000];

    setvbuf(stdout, NULL, _IONBF, 0);
    samp = LoadLibraryExA("samp.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
    check(samp != NULL, "samp.dll maps without running");
    if (!samp)
        return 1;
    base = (uintptr_t)samp;
    entry = *(uint32_t *)(base + *(uint32_t *)(base + 0x3C) + 0x28);
    for (i = 0; i < sizeof layouts / sizeof layouts[0]; i++)
        if (layouts[i].entry == entry)
            L = &layouts[i];
    if (!L) { printf("samp.dll build %x not covered\n", entry); return 1; }

    jump(base, L->show, s_show);         jump(base, L->close, s_close);
    jump(base, L->lb_sel, s_lb_sel);     jump(base, L->lb_select, s_lb_select);
    jump(base, L->eb_get, s_eb_get);     jump(base, L->eb_set, s_eb_set);
    jump(base, L->shutdown, s_shutdown); jump(base, L->cursor, s_cursor);
    jump(base, L->setname, s_setname);   jump(base, L->special, s_special);
    jump(base, L->td_set, s_td_set);     jump(base, L->vehicle_find, s_vehicle_find);
    jump(base, L->player_color, s_color);
    jump(base, L->add_message, s_add_message);
    jump(base, L->current_anim, s_current_anim);
    jump(base, L->anim_entry, s_anim_entry);
    *(void **)0xB74490 = &ped_pool;
    *(void **)0xB74494 = &vehicle_pool;
    *(int32_t *)0xC17044 = 1920;
    *(int32_t *)0xC17048 = 1080;

    for (i = 0; i < 40; i++)
        vtable[i] = (void *)f_nop;
    vtable[2] = (void *)f_disconnect;
    vtable[25] = (void *)f_rpc;
    AT(netgame, 0x2C, void *) = &rak;
    strcpy((char *)netgame + 0x30, "1.2.3.4");
    strcpy((char *)netgame + 0x131, "Test Server");
    AT(netgame, 0x235, int) = 7777;
    AT(netgame, 0x3CD, int) = 5;
    AT(netgame, 0x3D1, int) = 12345;
    AT(netgame, 0x3DE, void *) = pools;
    AT(pools, 0x08, void *) = pp;
    AT(pools, 0x18, void *) = gz_pool;
    AT(pools, 0x1C, void *) = label_pool;
    AT(pools, 0x20, void *) = td_pool;

    AT(pp, L->pp_largest, int) = 5;
    AT(pp, L->pp_local_id, uint16_t) = 3;
    set_name(pp + L->pp_local_name, "Me");
    AT(pp, L->pp_local_obj, void *) = lp;
    AT(pp, L->pp_local_ping, int) = 33;
    AT(pp, L->pp_used + 1 * 4, int) = 1;
    AT(pp, L->pp_info + 1 * 4, void *) = info1;
    AT(pp, L->pp_used + 4 * 4, int) = 1;
    AT(pp, L->pp_info + 4 * 4, void *) = info4;
    AT(info1, L->info_remote, void *) = remote1;
    set_name(info1 + L->info_name, "Alice");
    AT(info1, L->info_ping, int) = 42;
    AT(info4, L->info_remote, void *) = remote4;
    set_name(info4 + L->info_name, "Bob");
    AT(remote1, 0x1B0, float) = 75.5f;
    AT(remote1, 0x1AC, float) = 50.0f;
    AT(remote1, 0x18, uint8_t) = 23;
    AT(remote1, 0x1D1, int) = 2;
    AT(remote4, 0x1D1, int) = 0;
    {
        static uint8_t samp_ped[0x300];
        AT(remote1, 0, void *) = NULL;
        AT(remote1, L->remote_ped, void *) = samp_ped;
        AT(samp_ped, 0x2A4, void *) = peds + 2 * 0x7C4;
    }
    {
        static uint8_t vpool[0x4FB4 + 2000 * 4];
        AT(vpool, 0, int) = 5;
        AT(vpool, 0x4FB4 + 3 * 4, void *) = vehicles + 0xA18;
        AT(pools, 0x0C, void *) = vpool;
    }
    AT(lp, 0xF4, int) = 1;
    {
        static uint8_t local_ped[0x300];
        AT(lp, 0, void *) = local_ped;
    }
    AT(pp, L->local_score, int) = 12;
    AT(info1, L->info_score, int) = 24;
    AT(info4, L->info_npc, int) = 1;
    AT(remote1, L->remote_anim, uint32_t) = 0x00080007;   /* id 7, flags 8 */
    {   /* 3D text 9 */
        static char label_text[] = "Press 'H' to use elevator";
        uint8_t *l = label_pool + 9 * 0x1D;
        AT(l, 0, char *) = label_text;
        AT(l, 4, uint32_t) = 0xAB000000;
        AT(l, 8, float) = 1784.98f; AT(l, 0xC, float) = -1302.04f; AT(l, 0x10, float) = 13.65f;
        AT(l, 0x14, float) = 4.0f; AT(l, 0x18, uint8_t) = 0;   /* no LOS test: drawn behind walls */
        AT(l, 0x19, uint16_t) = 0xFFFF; AT(l, 0x1B, uint16_t) = 0xFFFF;
    }
    AT(label_pool, 0xE800 + 9 * 4, int) = 1;
    AT(td_pool, 7 * 4, int) = 1;
    AT(td_pool, 0x2400 + 7 * 4, void *) = td;
    strcpy((char *)td, "Hello ~r~there");
    AT(td, 0x963, float) = 0.5f;  AT(td, 0x967, float) = 1.5f;  AT(td, 0x96B, uint32_t) = 0xFF00FF00;
    AT(td, 0x971, uint8_t) = 1;   AT(td, 0x97A, uint32_t) = 0x80000000;
    AT(td, 0x972, float) = 640.0f; AT(td, 0x976, float) = 20.0f;
    AT(td, 0x98B, float) = 320.0f; AT(td, 0x98F, float) = 240.0f;
    AT(td, 0x986, uint8_t) = 1;  AT(td, 0x97E, uint8_t) = 1;  AT(td, 0x987, int) = 2;
    AT(td, 0x983, uint8_t) = 1;  AT(td, 0x97F, uint32_t) = 0xA6000000;
    AT(td, 0x9B6, float) = 1.0f; AT(td, 0x9BA, uint16_t) = 0xFFFF; AT(td, 0x9BC, uint16_t) = 0xFFFF;
    AT(dlg, 0x20, void *) = listbox;
    AT(dlg, 0x24, void *) = editbox;
    AT(dlg, 0x28, int) = 1; AT(dlg, 0x2C, int) = 2; AT(dlg, 0x30, int) = 1234; AT(dlg, 0x81, int) = 1;
    AT(scoreboard, 0, int) = 1;
    AT(chat, 8, int) = 2;

    set_global(base, L->netgame, netgame);
    set_global(base, L->chat, chat);
    set_global(base, L->input, input);
    set_global(base, L->dialog, dlg);
    set_global(base, L->game, game);
    set_global(base, L->scoreboard, scoreboard);

    check(LoadLibraryA("moonsamp.asi") != NULL, "moonsamp.asi loads");

    /* server */
    check(op_cond0(0x0AFA) == 1, "0AFA is_samp_available with the fakes up");
    check(op_out(0x0B4F) == 3, "0B4F gamestate 5 (connected) -> 3");
    begin(); put_int((int32_t)(uintptr_t)buf); op(0x0B3A);
    check(strcmp(buf, "Test Server") == 0, "0B3A server name into MoonLoader's buffer");
    begin(); put_int((int32_t)(uintptr_t)buf); put_local(0); op(0x0B39);
    check(strcmp(buf, "1.2.3.4") == 0 && LOCAL(0) == 7777, "0B39 address and port");

    /* chat: scripts pass RGB / ARGB, SA-MP's AddMessage takes RGBA */
    begin(); put_str("pink"); put_int(0xFF00FF); opbuf[n++] = 0; op(0x0AF8);
    check(chat_color == 0xFF00FFFF && strcmp(chat_text, "pink") == 0, "0AF8 0xFF00FF reaches AddMessage as RGBA FF00FFFF (pink, not green)");
    begin(); put_str("white"); put_int(-1); opbuf[n++] = 0; op(0x0AF8);
    check(chat_color == 0xFFFFFFFF, "0AF8 -1 stays white");

    /* players */
    check(op_cond0(0x0B61) == 1, "0B61 local player spawned");
    begin(); put_i8(0); put_local(0); op(0x0C8A);
    check(LOCAL(0) == 4, "0C8A highest connected id (4)");
    begin(); put_i8(1); put_local(0); op(0x0C8A);
    check(LOCAL(0) == 3, "0C8A streamed only: none streamed, so the local id");
    begin(); put_i8(0); put_local(0); op(0x0C8B);
    check(LOCAL(0) == 3, "0C8B two remote players plus the local one (NPCs counted, as SAMPFUNCS does)");
    check(op_int(0x0B2A, 1) == 42 && op_int(0x0B2A, 3) == 33, "0B2A ping, remote and local");
    check(op_cond(0x0B5C, 1) == 1 && op_cond(0x0B5C, 4) == 0, "0B5C paused is status 2");
    check(op_int(0x0B25, 1) == 75 && op_int(0x0B26, 1) == 50, "0B25 / 0B26 reported health and armour");
    check(op_int(0x0B62, 1) == 23, "0B62 remote special action");
    last.called = 0;
    check(op_int(0x0B62, 3) == 7 && last.called == 1 && last.self == lp,
          "0B62 local special action through CLocalPlayer::GetSpecialAction");
    begin(); put_int(1); put_local(0); op(0x0B36);
    check(strcmp((const char *)(uintptr_t)LOCAL(0), "Alice") == 0, "0B36 remote nickname");
    begin(); put_int(3); put_local(0); op(0x0B36);
    check(strcmp((const char *)(uintptr_t)LOCAL(0), "Me") == 0, "0B36 local nickname");
    check(op_cond(0x0B23, 1) == 1 && op_cond(0x0B23, 2) == 0, "0B23 connected");
    check((uint32_t)op_int(0x0B37, 1) == 0x00112233 && (uint32_t)op_int(0x0B37, 4) == 0xFFAABBCC,
          "0B37 RGBA -> ARGB keeps the alpha (0 hides a marker)");
    check(op_cond(0x0B46, 9) == 1 && op_cond(0x0B46, 8) == 0, "0B46 3D text defined");
    check(op_out(0x0BB5) == (int32_t)(uintptr_t)gz_pool, "0BB5 gangzone pool pointer");
    check(op_out(0x0BDC) == 2, "0BDC chat display mode");
    check(op_cond0(0x0B8C) == 0, "0B8C cursor off");
    AT(game, 0x61, int) = 2;
    check(op_cond0(0x0B8C) == 1, "0B8C cursor on");
    last.called = 0;
    begin(); put_int(3); op(0x0B8D);
    check(last.called == 1 && last.self == game && last.a == 3 && last.b == 0, "0B8D SetCursorMode(3, FALSE)");
    check(op_cond0(0x0BD5) == 1, "0BD5 scoreboard open");
    last.called = 0;
    begin(); put_str("NewName"); op(0x0B29);
    check(last.called == 1 && last.self == pp && strcmp(last.s[0], "NewName") == 0,
          "0B29 SetLocalPlayerName on the player pool");

    /* GTA handles (CPools::GetPed / GetPedRef / GetVehicle, cdecl) */
    {
        unsigned before = x87_tags();
        begin(); put_int(1); put_local(0); op(0x0B20);
        check(COND == 1 && LOCAL(0) == (2 << 8 | 0x05), "0B20 player 1 -> ped handle 0x205");
        begin(); put_int(2 << 8 | 0x05); put_local(0); op(0x0B2B);
        check(COND == 1 && LOCAL(0) == 1, "0B2B ped handle 0x205 -> player 1");
        begin(); put_int(2 << 8 | 0x06); put_local(0); op(0x0B2B);
        check(COND == 0 && LOCAL(0) == -1, "0B2B a stale handle is no player");
        last.called = 0;
        begin(); put_int(1 << 8 | 0x07); put_local(0); op(0x0B2C);
        check(COND == 1 && LOCAL(0) == 12, "0B2C car handle 0x107 -> vehicle 12");
        begin(); put_int(3); put_local(0); op(0x0AFF);
        check(COND == 1 && LOCAL(0) == (1 << 8 | 0x07), "0AFF vehicle 3 -> car handle 0x107");
        begin(); put_int(4); put_local(0); op(0x0AFF);
        check(COND == 0 && LOCAL(0) == -1, "0AFF an empty vehicle slot is no handle");
        begin(); put_local(0); put_local(1); op(0x0B5A);
        check(LOCAL(0) == 1920 && LOCAL(1) == 1080, "0B5A width in local 0, height in local 1");
        check(x87_tags() == before && before == 0xFFFF, "the x87 stack is still empty after them");
    }

    /* dialogs */
    check(op_cond(0x0B4C, -1) == 1 && op_cond(0x0B4C, 1234) == 1 && op_cond(0x0B4C, 99) == 0,
          "0B4C dialog active, any id or this one");
    check(op_out(0x0B4E) == 1234 && op_out(0x0B4D) == 2, "0B4E id, 0B4D type");
    last.called = 0;
    begin(); put_int(77); put_str("Cap"); put_str("Body"); put_str("OK"); put_str("No"); put_int(1); op(0x0B3B);
    check(last.called == 1 && last.self == dlg && last.a == 77 && last.b == 1 &&
          strcmp(last.s[0], "Cap") == 0 && strcmp(last.s[1], "Body") == 0 &&
          strcmp(last.s[2], "OK") == 0 && strcmp(last.s[3], "No") == 0,
          "0B3B CDialog::Show(77, style 1, caption, text, buttons)");
    check(AT(dlg, 0x81, int) == 2, "a script's dialog is marked m_bServerside = 2");

    /* RakNet hooks in place (any hook setup installs them) */
    begin(); put_int(-16); op(0x0BE4);
    {
        int (THISCALL *rpc)(void *, int *, void *, int, int, int, int) = vtable[25];
        struct { int used, alloc, read; uint8_t *data; uint8_t copy, stack[256]; } __attribute__((packed)) bs;
        static uint8_t resp[] = { 77, 0, 1, 3, 0, 2, 'h', 'i' };
        int id = 62;
        memset(&bs, 0, sizeof bs);
        bs.used = bs.alloc = sizeof resp * 8;
        bs.data = resp;
        rpc_calls = 0;
        check(rpc != (void *)f_rpc, "RakClient RPC slot hooked");
        rpc(&rak, &id, &bs, 1, 9, 0, 0);
        check(rpc_calls == 0, "its DialogResponse stays off the server");
        begin(); put_int(77); put_local(0); put_local(1); put_int((int32_t)(uintptr_t)buf); op(0x0B3C);
        check(COND == 1 && LOCAL(0) == 1 && LOCAL(1) == 3 && strcmp(buf, "hi") == 0,
              "0B3C reports button 1, item 3, \"hi\"");
        begin(); put_int(77); put_local(0); put_local(1); put_int((int32_t)(uintptr_t)buf); op(0x0B3C);
        check(COND == 0, "0B3C reports it once");
        begin(); put_int(77); put_int(0); put_int(-1); put_str("x"); op(0x0BC9);
        check(rpc_calls == 1 && rpc_id == 62 && memcmp(rpc_data, "\x4D\x00\x00\xFF\xFF\x01x", 7) == 0,
              "0BC9 sends DialogResponse 4D 00 | 00 | FF FF | 01 'x' to the server");
        begin(); put_int(0); op(0x0BD9);
        check(AT(dlg, 0x81, int) == 1, "0BD9(false) makes it server-side");
        rpc(&rak, &id, &bs, 1, 9, 0, 0);
        check(rpc_calls == 2, "a server-side dialog's response goes out");
    }
    last.called = 0;
    begin(); put_int(1); op(0x0B47);
    check(last.called == 1 && last.self == dlg && last.a == 1, "0B47 CDialog::Close(1)");
    last.called = 0;
    check(op_out(0x0B48) == 5 && last.self == listbox && last.a == -1, "0B48 GetSelectedIndex(-1)");
    last.called = 0;
    begin(); put_int(2); op(0x0B49);
    check(last.called == 1 && last.self == listbox && last.a == 2, "0B49 SelectItem(2)");
    begin(); put_int((int32_t)(uintptr_t)buf); op(0x0B4A);
    check(strcmp(buf, "typed") == 0, "0B4A editbox text into MoonLoader's buffer");
    last.called = 0;
    begin(); put_str("pw"); op(0x0B4B);
    check(last.called == 1 && last.self == editbox && strcmp(last.s[0], "pw") == 0 && last.a == 0,
          "0B4B SetText(\"pw\", FALSE)");

    /* textdraws */
    check(op_cond(0x0C5D, 7) == 1 && op_cond(0x0C5D, 8) == 0 && op_cond(0x0C5D, 5000) == 0,
          "0C5D exists");
    begin(); put_int(7); put_int((int32_t)(uintptr_t)buf); op(0x0C5A);
    check(strcmp(buf, "Hello ~r~there") == 0, "0C5A text into MoonLoader's buffer");
    last.called = 0;
    begin(); put_int(7); put_str("New"); op(0x0C50);
    check(last.called == 1 && last.self == td && strcmp(last.s[0], "New") == 0, "0C50 CTextDraw::SetText");
    begin(); put_int(7); put_local(0); put_local(1); op(0x0C5B);
    check(LOCALF(0) == 320.0f && LOCALF(1) == 240.0f, "0C5B position");
    begin(); put_int(7); put_float(10.0f); put_float(20.0f); op(0x0C51);
    check(AT(td, 0x98B, float) == 10.0f && AT(td, 0x98F, float) == 20.0f, "0C51 sets the position");
    begin(); put_int(7); put_local(0); put_local(1); put_local(2); op(0x0C5C);
    AT(td, 0x96B, uint32_t) = 0xFF2A45FF;   /* ABGR, as SA-MP keeps it */
    begin(); put_int(7); put_local(0); put_local(1); put_local(2); op(0x0C5C);
    check(LOCALF(0) == 0.5f && LOCALF(1) == 1.5f && (uint32_t)LOCAL(2) == 0xFFFF452A, "0C5C letter size, colour as ARGB");
    begin(); put_int(7); put_float(0.25f); put_float(1.0f); put_int(0x11223344); op(0x0C52);
    check(AT(td, 0x963, float) == 0.25f && AT(td, 0x96B, uint32_t) == 0x11443322, "0C52 sets them (ARGB in, ABGR stored)");
    begin(); put_int(7); put_local(0); put_local(1); put_local(2); put_local(3); op(0x0C53);
    check(LOCAL(0) == 1 && (uint32_t)LOCAL(1) == 0x80000000 && LOCALF(2) == 640.0f && LOCALF(3) == 20.0f,
          "0C53 box, colour, size");
    begin(); put_int(7); put_int(0); put_int(0x55); put_float(1.0f); put_float(2.0f); op(0x0C49);
    check(AT(td, 0x971, uint8_t) == 0 && AT(td, 0x97A, uint32_t) == 0x550000 && AT(td, 0x976, float) == 2.0f,
          "0C49 sets them (ARGB in, ABGR stored)");

    /* more getters (from the SAMPFUNCS comparison) */
    begin(); put_int(7); put_local(0); op(0x0C54); check(LOCAL(0) == 3, "0C54 align: right = 3");
    begin(); put_int(7); put_local(0); op(0x0C55); check(LOCAL(0) == 1, "0C55 proportional");
    begin(); put_int(7); put_local(0); op(0x0C56); check(LOCAL(0) == 2, "0C56 style");
    begin(); put_int(7); put_local(0); put_local(1); op(0x0C57);
    check(LOCAL(0) == 1 && (uint32_t)LOCAL(1) == 0xA6000000, "0C57 shadow and its colour");
    begin(); put_int(7); put_local(0); put_local(1); put_local(2); put_local(3); put_local(4); put_local(5); put_local(6); op(0x0C59);
    check(LOCAL(0) == 0 && LOCALF(4) == 1.0f && LOCAL(5) == 0xFFFF && LOCAL(6) == 0xFFFF, "0C59 model, rotation, zoom, colours");
    begin(); put_int(9);
    for (i = 0; i < 9; i++) put_local((int)(i == 0 ? 8 : i - 1));
    op(0x0C46);
    check(strcmp((const char *)(uintptr_t)LOCAL(8), "Press 'H' to use elevator") == 0 && (uint32_t)LOCAL(0) == 0xAB000000 &&
          LOCALF(4) == 4.0f && LOCAL(5) == 1 && LOCAL(6) == 0xFFFF, "0C46 3D text info, text in local 8, behind walls");
    {   /* chat line 99 */
        char text[160], prefix[40];
        uint8_t *e = chat + 0x132 + 99 * 0xFC;
        strcpy((char *)e + 0x20, "Organization: Higher Croft Crips Family.");
        strcpy((char *)e + 0x04, "[R]");
        AT(e, 0xF4, uint32_t) = 0xFF00FF00; AT(e, 0xF8, uint32_t) = 0xFFAA0000;
        begin(); put_int(99); put_int((int32_t)(uintptr_t)text); put_int((int32_t)(uintptr_t)prefix); put_local(0); put_local(1); op(0x0B75);
        check(strcmp(text, "Organization: Higher Croft Crips Family.") == 0 && strcmp(prefix, "[R]") == 0 &&
              (uint32_t)LOCAL(0) == 0xFF00FF00 && (uint32_t)LOCAL(1) == 0xFFAA0000, "0B75 chat line 99: text, prefix, colours");
    }
    check(op_int(0x0B65, 1) == 24 && op_int(0x0B65, 3) == 12, "0B65 score, remote and local");
    check(op_cond(0x0B64, 4) == 1 && op_cond(0x0B64, 1) == 0, "0B64 NPC flag");
    AT(remote4, 0x1D1, int) = 2;
    check(op_cond(0x0B5C, 4) == 0, "0B5C status 2 without a streamed ped is not paused");
    AT(remote4, 0x1D1, int) = 0;
    begin(); put_int(3); put_local(0); op(0x0B20);
    check(COND == 0, "0B20 gives no handle for the local player (as SAMPFUNCS)");
    check(op_int(0x0B57, 3) == 1189 && anim_ped == AT(lp, 0, void *), "0B57 local animation from CPed::GetCurrentAnimation");
    check(op_int(0x0B57, 1) == 7, "0B57 remote animation from its on-foot sync");
    {
        char nm[64], fl[64];
        begin(); put_int(1189); put_int((int32_t)(uintptr_t)nm); put_int((int32_t)(uintptr_t)fl); op(0x0B58);
        check(COND == 1 && strcmp(nm, "IDLE_STANCE") == 0 && strcmp(fl, "PED") == 0, "0B58 animation 1189 is PED:IDLE_STANCE");
        begin(); put_str("barman_idle"); put_str("BAR"); put_local(0); op(0x0B59);
        check(COND == 1 && LOCAL(0) == 7, "0B59 finds BAR:BARMAN_IDLE as 7");
    }
    begin(); put_int(25); put_local(0); op(0x0BF8);
    check(strcmp((const char *)(uintptr_t)LOCAL(0), "ClientJoin") == 0, "0BF8 RPC 25 is ClientJoin");
    begin(); put_int(207); put_local(0); op(0x0BF9);
    check(strcmp((const char *)(uintptr_t)LOCAL(0), "PLAYER_SYNC") == 0, "0BF9 packet 207 is PLAYER_SYNC");
    check(op_cond0(0x0BDB) == 1, "0BDB chat visible (mode 2)");
    check(op_out(0x0B8E) == 2, "0B8E cursor mode");
    AT(dlg, 0x81, int) = 0;
    check(op_cond0(0x0BDA) == 1, "0BDA a dialog not answered to the server is client-side");
    AT(dlg, 0x81, int) = 1;
    check(op_cond0(0x0BDA) == 0, "0BDA a server dialog is not");

    /* reconnect */
    last.called = 0;
    begin(); put_str("5.6.7.8"); put_int(1234); op(0x0B38);
    check(disc_calls == 1 && disc_block == 500, "0B38 disconnects RakNet (500 ms)");
    check(last.called == 1 && last.self == netgame, "0B38 resets as for a server restart");
    check(strcmp((char *)netgame + 0x30, "5.6.7.8") == 0 && AT(netgame, 0x235, int) == 1234 &&
          AT(netgame, 0x3CD, int) == 1 && AT(netgame, 0x3D1, int) == 0,
          "0B38 sets the address and waits to connect");

    printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
