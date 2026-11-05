/* What a script sends to the server as the client would: RPCs laid out as
 * SA-MP's own senders write them, sync packets as their raw structs, and
 * the last sync data received from other players. */
#include "moonsamp.h"

#define HIGH_RELIABLE        8          /* RELIABLE */
#define UNRELIABLE_SEQUENCED 7

enum {
    RPC_CLICK_PLAYER = 23, RPC_ENTER_VEHICLE = 26, RPC_SPAWN = 52, RPC_DEATH = 53,
    RPC_CLICK_TEXTDRAW = 83, RPC_SCM_EVENT = 96, RPC_DAMAGE_VEHICLE = 106, RPC_GIVE_TAKE_DAMAGE = 115,
    RPC_EDIT_ATTACHED_OBJECT = 116, RPC_EDIT_OBJECT = 117, RPC_SET_INTERIOR = 118,
    RPC_REQUEST_CLASS = 128, RPC_REQUEST_SPAWN = 129, RPC_PICKED_UP_PICKUP = 131,
    RPC_MENU_SELECT = 132, RPC_VEHICLE_DESTROYED = 136, RPC_MENU_QUIT = 140, RPC_EXIT_VEHICLE = 154,
};

enum {
    ID_VEHICLE_SYNC = 200, ID_RCON_COMMAND = 201, ID_AIM_SYNC = 203, ID_BULLET_SYNC = 206,
    ID_PLAYER_SYNC = 207, ID_UNOCCUPIED_SYNC = 209, ID_TRAILER_SYNC = 210,
    ID_PASSENGER_SYNC = 211, ID_SPECTATOR_SYNC = 212,
};

/* sync structs, as SA-MP sends and keeps them */
#define ONFOOT_SIZE     0x44
#define INCAR_SIZE      0x3F
#define PASSENGER_SIZE  0x18
#define AIM_SIZE        0x1F
#define TRAILER_SIZE    0x36
#define UNOCCUPIED_SIZE 0x43
#define BULLET_SIZE     0x28
#define SPECTATOR_SIZE  0x12

/* ---- writing, RakNet's way: each value's bytes in memory order, each byte
 * from its high bit; a bool is one bit ------------------------------------ */

typedef struct { uint8_t d[512]; int bits; } Out;

static void out_bit(Out *o, int v)
{
    if (o->bits >= (int)sizeof o->d * 8)
        return;
    if (v)
        o->d[o->bits >> 3] |= (uint8_t)(0x80 >> (o->bits & 7));
    o->bits++;
}

static void out_bytes(Out *o, const void *p, int n)
{
    const uint8_t *b = p;
    int i, k;
    for (i = 0; i < n; i++)
        for (k = 7; k >= 0; k--)
            out_bit(o, b[i] >> k & 1);
}

static void out_u8(Out *o, uint32_t v)  { uint8_t x = (uint8_t)v; out_bytes(o, &x, 1); }
static void out_u16(Out *o, uint32_t v) { uint16_t x = (uint16_t)v; out_bytes(o, &x, 2); }
static void out_u32(Out *o, uint32_t v) { out_bytes(o, &v, 4); }
static void out_f32(Out *o, float v)    { out_bytes(o, &v, 4); }

static void rpc(int id, Out *o) { raknet_send(id, o->d, o->bits, 0); }

static Out *fresh(void)
{
    static Out o;
    memset(&o, 0, sizeof o);
    return &o;
}

/* An RPC of the script's int parameters, each written with its width
 * (1, 2 or 4 bytes). */
static int8_t ints_rpc(Script *s, int id, const char *widths)
{
    Out *o = fresh();
    int n = lstrlenA(widths), i;
    script_collect(s, n);
    for (i = 0; i < n; i++) {
        uint32_t v = ScriptParams[i].u;
        if (widths[i] == '1') out_u8(o, v);
        else if (widths[i] == '2') out_u16(o, v);
        else out_u32(o, v);
    }
    rpc(id, o);
    return 0;
}

/* 0AFB samp request_class %1d% */
static int8_t __stdcall op_0AFB(Script *s) { return ints_rpc(s, RPC_REQUEST_CLASS, "4"); }
/* 0AFC samp send_scm_event %1d% event_id %2d% param_1 %3d% param_2 %4d% */
static int8_t __stdcall op_0AFC(Script *s) { return ints_rpc(s, RPC_SCM_EVENT, "4444"); }
/* 0B30 samp send_enter_vehicle %1d% as_passenger %2d% */
static int8_t __stdcall op_0B30(Script *s) { return ints_rpc(s, RPC_ENTER_VEHICLE, "21"); }
/* 0B31 samp send_exit_vehicle %1d% */
static int8_t __stdcall op_0B31(Script *s) { return ints_rpc(s, RPC_EXIT_VEHICLE, "2"); }
/* 0B32 samp send_spawn, 0BD0 send_request_spawn, 0BD3 send_menu_quit */
static int8_t __stdcall op_0B32(Script *s) { return ints_rpc(s, RPC_SPAWN, ""); }
static int8_t __stdcall op_0BD0(Script *s) { return ints_rpc(s, RPC_REQUEST_SPAWN, ""); }
static int8_t __stdcall op_0BD3(Script *s) { return ints_rpc(s, RPC_MENU_QUIT, ""); }
/* 0BC8 samp send_click_player %1d% source %2d% */
static int8_t __stdcall op_0BC8(Script *s) { return ints_rpc(s, RPC_CLICK_PLAYER, "21"); }
/* 0BCA samp send_click_textdraw %1d% */
static int8_t __stdcall op_0BCA(Script *s) { return ints_rpc(s, RPC_CLICK_TEXTDRAW, "2"); }
/* 0BCF samp send_interior_change %1d% */
static int8_t __stdcall op_0BCF(Script *s) { return ints_rpc(s, RPC_SET_INTERIOR, "1"); }
/* 0BD1 samp send_picked_up_pickup %1d% */
static int8_t __stdcall op_0BD1(Script *s) { return ints_rpc(s, RPC_PICKED_UP_PICKUP, "4"); }
/* 0BD2 samp send_menu_select_row %1d% */
static int8_t __stdcall op_0BD2(Script *s) { return ints_rpc(s, RPC_MENU_SELECT, "1"); }
/* 0BD4 samp send_vehicle_destroyed %1d% */
static int8_t __stdcall op_0BD4(Script *s) { return ints_rpc(s, RPC_VEHICLE_DESTROYED, "2"); }

/* 0AFE samp send_death_by_player %1d% with_reason %2d%: reason, then killer */
static int8_t __stdcall op_0AFE(Script *s)
{
    Out *o = fresh();
    script_collect(s, 2);
    out_u8(o, ScriptParams[1].u);
    out_u16(o, ScriptParams[0].u);
    rpc(RPC_DEATH, o);
    return 0;
}

/* 0B33 samp send_damage_vehicle %1d% panel %2d% doors %3d% lights %4d% tires %5d%:
 * the vehicle as a GTA handle */
static int8_t __stdcall op_0B33(Script *s)
{
    Out *o = fresh();
    int id;
    script_collect(s, 5);
    if ((id = samp_vehicle_id(ScriptParams[0].i)) < 0)
        return 0;
    out_u16(o, id);
    out_u32(o, ScriptParams[1].u);
    out_u32(o, ScriptParams[2].u);
    out_u8(o, ScriptParams[3].u);
    out_u8(o, ScriptParams[4].u);
    rpc(RPC_DAMAGE_VEHICLE, o);
    return 0;
}

/* 0BCB samp send_give_damage %1d% damage %2d% weapon %3d% bodypart %4d%,
 * 0BCC ... take_damage: a bit for taking, player, amount, weapon, body part */
static int8_t damage(Script *s, int taking)
{
    Out *o = fresh();
    script_collect(s, 4);
    out_bit(o, taking);
    out_u16(o, ScriptParams[0].u);
    out_f32(o, ScriptParams[1].f);
    out_u32(o, ScriptParams[2].u);
    out_u32(o, ScriptParams[3].u);
    rpc(RPC_GIVE_TAKE_DAMAGE, o);
    return 0;
}
static int8_t __stdcall op_0BCB(Script *s) { return damage(s, 0); }
static int8_t __stdcall op_0BCC(Script *s) { return damage(s, 1); }

/* 0BCD samp send_edit_object playerobject %1d% object %2d% response %3d% pos %4d% %5d% %6d% rotation %7d% %8d% %9d% */
static int8_t __stdcall op_0BCD(Script *s)
{
    Out *o = fresh();
    int i;
    script_collect(s, 9);
    out_bit(o, ScriptParams[0].i != 0);
    out_u16(o, ScriptParams[1].u);
    out_u32(o, ScriptParams[2].u);
    for (i = 3; i < 9; i++)
        out_f32(o, ScriptParams[i].f);
    rpc(RPC_EDIT_OBJECT, o);
    return 0;
}

/* 0BCE samp send_edit_attached_object response %1d% index %2d% model %3d% bone %4d%
 * offset %5d% %6d% %7d% rot %8d% %9d% %10d% scale %11d% %12d% %13d%: the
 * material colours SA-MP adds are 0 */
static int8_t __stdcall op_0BCE(Script *s)
{
    Out *o = fresh();
    int i;
    script_collect(s, 13);
    for (i = 0; i < 4; i++)
        out_u32(o, ScriptParams[i].u);
    for (; i < 13; i++)
        out_f32(o, ScriptParams[i].f);
    out_u32(o, 0);
    out_u32(o, 0);
    rpc(RPC_EDIT_ATTACHED_OBJECT, o);
    return 0;
}

/* 0BBF samp send_rcon_command %1d% */
static int8_t __stdcall op_0BBF(Script *s)
{
    char buf[256];
    const char *cmd = script_string(s, buf, sizeof buf);
    Out *o = fresh();
    int len = lstrlenA(cmd);
    if (len > 255)
        len = 255;
    out_u8(o, ID_RCON_COMMAND);
    out_u32(o, (uint32_t)len);
    out_bytes(o, cmd, len);
    raknet_send(-1, o->d, o->bits, HIGH_RELIABLE);
    return 0;
}

/* 0BC0-0BC7 samp send_*_data %1d%: the packet id, then the struct */
static int8_t sync_packet(Script *s, int id, int size)
{
    const void *data = (const void *)(uintptr_t)script_int(s);
    Out *o = fresh();
    if (!data)
        return 0;
    out_u8(o, id);
    out_bytes(o, data, size);
    raknet_send(-1, o->d, o->bits, UNRELIABLE_SEQUENCED);
    return 0;
}
static int8_t __stdcall op_0BC0(Script *s) { return sync_packet(s, ID_PLAYER_SYNC, ONFOOT_SIZE); }
static int8_t __stdcall op_0BC1(Script *s) { return sync_packet(s, ID_VEHICLE_SYNC, INCAR_SIZE); }
static int8_t __stdcall op_0BC2(Script *s) { return sync_packet(s, ID_PASSENGER_SYNC, PASSENGER_SIZE); }
static int8_t __stdcall op_0BC3(Script *s) { return sync_packet(s, ID_AIM_SYNC, AIM_SIZE); }
static int8_t __stdcall op_0BC4(Script *s) { return sync_packet(s, ID_BULLET_SYNC, BULLET_SIZE); }
static int8_t __stdcall op_0BC5(Script *s) { return sync_packet(s, ID_TRAILER_SYNC, TRAILER_SIZE); }
static int8_t __stdcall op_0BC6(Script *s) { return sync_packet(s, ID_UNOCCUPIED_SYNC, UNOCCUPIED_SIZE); }
static int8_t __stdcall op_0BC7(Script *s) { return sync_packet(s, ID_SPECTATOR_SYNC, SPECTATOR_SIZE); }

/* 0BBA-0BBE samp store_player %1d% *_data %2d%: the last sync data received
 * from that player, into the script's buffer */
static int8_t store_sync(Script *s, int which, int size)
{
    const Build *b = samp_build();
    uint8_t *rp, *dst;
    int off;
    script_collect(s, 2);
    dst = ScriptParams[1].p;
    if (!dst || !b)
        return 0;
    rp = samp_remote_player(ScriptParams[0].i);
    switch (which) {
    case 0: off = b->remote_onfoot; break;
    case 1: off = b->remote_incar; break;
    case 2: off = b->remote_passenger; break;
    case 3: off = b->remote_incar + INCAR_SIZE; break;    /* trailer */
    default: off = b->remote_aim; break;
    }
    if (rp)
        memcpy(dst, rp + off, size);
    else
        memset(dst, 0, size);
    return 0;
}
static int8_t __stdcall op_0BBA(Script *s) { return store_sync(s, 0, ONFOOT_SIZE); }
static int8_t __stdcall op_0BBB(Script *s) { return store_sync(s, 1, INCAR_SIZE); }
static int8_t __stdcall op_0BBC(Script *s) { return store_sync(s, 2, PASSENGER_SIZE); }
static int8_t __stdcall op_0BBD(Script *s) { return store_sync(s, 3, TRAILER_SIZE); }
static int8_t __stdcall op_0BBE(Script *s) { return store_sync(s, 4, AIM_SIZE); }

/* 0B28 samp disconnect_with_reason %1d%: a timeout leaves without telling
 * the server */
static int8_t __stdcall op_0B28(Script *s)
{
    typedef void (THISCALL *DisconnectFn)(void *, unsigned, unsigned char);
    int timeout = script_int(s) != 0;
    void *rc = samp_rakclient();
    if (rc)
        ((DisconnectFn)(*(void ***)rc)[2])(rc, timeout ? 0 : 500, 0);
    return 0;
}

/* ---- SA-MP's own sync senders, called now ------------------------------ */

/* Collects n parameters and calls CLocalPlayer's sender with the first
 * `pass` of them (a vehicle id, then a seat). */
static int8_t force(Script *s, int n, int pass, uint32_t field)
{
    const Build *b = samp_build();
    void *lp = samp_local_player();
    uint32_t rva;
    if (n)
        script_collect(s, n);
    if (!b || !lp)
        return 0;
    rva = *(const uint32_t *)((const uint8_t *)b + field);
    if (pass == 0)
        ((void (THISCALL *)(void *))(samp_base + rva))(lp);
    else if (pass == 1)
        ((void (THISCALL *)(void *, int))(samp_base + rva))(lp, ScriptParams[0].i & 0xFFFF);
    else
        ((void (THISCALL *)(void *, int, int))(samp_base + rva))(lp, ScriptParams[0].i & 0xFFFF, ScriptParams[1].i & 0xFF);
    return 0;
}

#define SENDER(f) __builtin_offsetof(Build, f)

/* 0C81 samp force_vehicle_sync %1d%: SA-MP sends the vehicle the player drives */
static int8_t __stdcall op_0C81(Script *s) { return force(s, 1, 0, SENDER(send_incar)); }
/* 0C82 samp force_unoccupied_sync %1d% seat_id %2d% */
static int8_t __stdcall op_0C82(Script *s) { return force(s, 2, 2, SENDER(send_unoccupied)); }
/* 0C83 samp force_onfoot_sync, 0C84 force_aim_sync */
static int8_t __stdcall op_0C83(Script *s) { return force(s, 0, 0, SENDER(send_onfoot)); }
static int8_t __stdcall op_0C84(Script *s) { return force(s, 0, 0, SENDER(send_aim)); }
/* 0C85 samp force_trailer_sync %1d% */
static int8_t __stdcall op_0C85(Script *s) { return force(s, 1, 1, SENDER(send_trailer)); }
/* 0C86 samp force_passenger_sync %1d% seat_id %2d%: SA-MP sends the seat the player is in */
static int8_t __stdcall op_0C86(Script *s) { return force(s, 2, 0, SENDER(send_passenger)); }
/* 0C87 samp force_stats_sync, 0C88 force_weapons_sync */
static int8_t __stdcall op_0C87(Script *s) { return force(s, 0, 0, SENDER(send_stats)); }
static int8_t __stdcall op_0C88(Script *s) { return force(s, 0, 0, SENDER(update_weapons)); }

void sends_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0AFB, op_0AFB }, { 0x0AFC, op_0AFC }, { 0x0B30, op_0B30 }, { 0x0B31, op_0B31 },
        { 0x0B32, op_0B32 }, { 0x0BD0, op_0BD0 }, { 0x0BD3, op_0BD3 }, { 0x0BC8, op_0BC8 },
        { 0x0BCA, op_0BCA }, { 0x0BCF, op_0BCF }, { 0x0BD1, op_0BD1 }, { 0x0BD2, op_0BD2 },
        { 0x0BD4, op_0BD4 }, { 0x0AFE, op_0AFE }, { 0x0B33, op_0B33 }, { 0x0BCB, op_0BCB },
        { 0x0BCC, op_0BCC }, { 0x0BCD, op_0BCD }, { 0x0BCE, op_0BCE }, { 0x0BBF, op_0BBF },
        { 0x0BC0, op_0BC0 }, { 0x0BC1, op_0BC1 }, { 0x0BC2, op_0BC2 }, { 0x0BC3, op_0BC3 },
        { 0x0BC4, op_0BC4 }, { 0x0BC5, op_0BC5 }, { 0x0BC6, op_0BC6 }, { 0x0BC7, op_0BC7 },
        { 0x0BBA, op_0BBA }, { 0x0BBB, op_0BBB }, { 0x0BBC, op_0BBC }, { 0x0BBD, op_0BBD },
        { 0x0BBE, op_0BBE }, { 0x0B28, op_0B28 }, { 0x0C81, op_0C81 }, { 0x0C82, op_0C82 },
        { 0x0C83, op_0C83 }, { 0x0C84, op_0C84 }, { 0x0C85, op_0C85 }, { 0x0C86, op_0C86 },
        { 0x0C87, op_0C87 }, { 0x0C88, op_0C88 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
