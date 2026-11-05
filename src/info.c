/* SA-MP's objects as raw pointers, and the getters and setters that need
 * no SA-MP function of their own. */
#include "moonsamp.h"

#define DLG_TEXT          0x34      /* char * */
#define DLG_CAPTION       0x40      /* char[65] */
#define INPUT_EDITBOX     0x08
#define CHAT_MODE         0x08
#define CHAT_ENTRIES      0x132
#define CHAT_ENTRY_SIZE   0xFC
#define CHAT_REDRAW       0x63DA
#define ENTITY_HANDLE     0x44      /* CEntity: the GTA script handle */
#define PICKUPS           4096
#define PICKUP_HANDLES    0x04
#define CURSOR_LOCKCAM    3
#define DLG_LISTBOX       0x20
#define LB_ITEMS          0x14C     /* CDXUTListBox: DXUTListBoxItem *[], */
#define LB_COUNT          0x150     /* their count; an item starts with char text[256] */
#define RPC_NODES         0x801     /* RakPeer: RPCNode *[256]; a node's handler at +1 */

typedef const char *(THISCALL *GetTextFn)(void *);
typedef void (THISCALL *SetTextFn)(void *, const char *, int);

static const Build *b(void) { return samp_build(); }
static void *fn(uint32_t rva) { return (void *)(samp_base + rva); }

static void store1(Script *s, int32_t v)
{
    ScriptParams[0].i = v;
    script_store(s, 1);
}

static void store_ptr(Script *s, const void *p) { store1(s, (int32_t)(uintptr_t)p); }

/* ---- pointers ---------------------------------------------------------- */

/* 0AF7 samp %1d% = get_base */
static int8_t __stdcall op_0AF7(Script *s) { store_ptr(s, GetModuleHandleA("samp.dll")); return 0; }

/* 0B7F get_samp_info_ptr, 0BAE chat, 0BAF input, 0BB0 dialog, 0BB1 kill
 * list, 0BB2 misc (CGame) */
static int8_t __stdcall op_0B7F(Script *s) { store_ptr(s, samp_netgame()); return 0; }
static int8_t __stdcall op_0BAE(Script *s) { store_ptr(s, b() ? samp_global(b()->chat) : NULL); return 0; }
static int8_t __stdcall op_0BAF(Script *s) { store_ptr(s, b() ? samp_global(b()->input) : NULL); return 0; }
static int8_t __stdcall op_0BB0(Script *s) { store_ptr(s, b() ? samp_global(b()->dialog) : NULL); return 0; }
static int8_t __stdcall op_0BB1(Script *s) { store_ptr(s, b() ? samp_global(b()->death_window) : NULL); return 0; }
static int8_t __stdcall op_0BB2(Script *s) { store_ptr(s, b() ? samp_global(b()->game) : NULL); return 0; }

/* 0BAC get_server_settings_ptr: CNetGame's settings follow the last connect attempt */
static int8_t __stdcall op_0BAC(Script *s)
{
    void *ng = samp_netgame();
    store_ptr(s, ng ? AT(ng, b()->ng_state + 8, void *) : NULL);
    return 0;
}

/* 0BAD get_samp_pools_ptr */
static int8_t __stdcall op_0BAD(Script *s)
{
    void *ng = samp_netgame();
    store_ptr(s, ng ? AT(ng, b()->ng_pools, void *) : NULL);
    return 0;
}

/* 0BB3 textdraw, 0BB4 object, 0BB6 3D text, 0BB7 player, 0BB8 vehicle, 0BB9 pickup pool */
static int8_t __stdcall op_0BB3(Script *s) { store_ptr(s, samp_pool(POOL_TEXTDRAW)); return 0; }
static int8_t __stdcall op_0BB4(Script *s) { store_ptr(s, samp_pool(POOL_OBJECT)); return 0; }
static int8_t __stdcall op_0BB6(Script *s) { store_ptr(s, samp_pool(POOL_LABEL)); return 0; }
static int8_t __stdcall op_0BB7(Script *s) { store_ptr(s, samp_pool(POOL_PLAYER)); return 0; }
static int8_t __stdcall op_0BB8(Script *s) { store_ptr(s, samp_pool(POOL_VEHICLE)); return 0; }
static int8_t __stdcall op_0BB9(Script *s) { store_ptr(s, samp_pool(POOL_PICKUP)); return 0; }

/* 0B7A samp %1d% = get_rakclient_interface */
static int8_t __stdcall op_0B7A(Script *s) { store_ptr(s, samp_rakclient()); return 0; }

/* 0B7C samp %2d% = get_rakclient_func_by_index %1d% */
static int8_t __stdcall op_0B7C(Script *s)
{
    int i = script_int(s);
    void *rc = samp_rakclient();
    store_ptr(s, rc && i >= 0 && i < 64 ? (*(void ***)rc)[i] : NULL);
    return 0;
}

/* 0B7B samp %1d% = get_rakpeer */
static int8_t __stdcall op_0B7B(Script *s) { store_ptr(s, samp_rakpeer()); return 0; }

static void *rpc_node(int id)
{
    uint8_t *peer = samp_rakpeer();
    return peer && id >= 0 && id < 256 ? AT(peer, RPC_NODES + id * 4, void *) : NULL;
}

/* 0B7D samp %2d% = get_rpc_func_by_index %1d%, 0B7E ... get_rpc_node_by_index */
static int8_t __stdcall op_0B7D(Script *s)
{
    uint8_t *node = rpc_node(script_int(s));
    store_ptr(s, node ? AT(node, 1, void *) : NULL);
    return 0;
}

static int8_t __stdcall op_0B7E(Script *s) { store_ptr(s, rpc_node(script_int(s))); return 0; }

/* 0B24 samp %2d% = get_player_struct_ptr %1d%: SAMPFUNCS's stRemotePlayer,
 * SA-MP's CPlayerInfo */
static int8_t __stdcall op_0B24(Script *s)
{
    int id = script_int(s);
    uint8_t *pp = samp_pool(POOL_PLAYER);
    void *info = NULL;
    if (pp && id >= 0 && id < 1004 && AT(pp, b()->pp_used + id * 4, int) == 1)
        info = AT(pp, b()->pp_info + id * 4, void *);
    store_ptr(s, info);
    return 0;
}

/* ---- objects and pickups ---------------------------------------------- */

static int object_max(void) { return (b()->obj_objects - 4) / 4; }

static uint8_t *object(int id)
{
    uint8_t *pool = samp_pool(POOL_OBJECT);
    if (!pool || id < 0 || id >= object_max() || !AT(pool, 4 + id * 4, int))
        return NULL;
    return AT(pool, b()->obj_objects + id * 4, uint8_t *);
}

/* 0B50 samp %2d% = object_handle_by_id %1d% */
static int8_t __stdcall op_0B50(Script *s)
{
    uint8_t *o = object(script_int(s));
    int h = o ? AT(o, ENTITY_HANDLE, int) : -1;
    store1(s, h);
    script_cond(s, o != NULL);
    return 0;
}

/* 0B52 samp %2d% = object_id_by_handle %1d% */
static int8_t __stdcall op_0B52(Script *s)
{
    int h = script_int(s), id, found = -1;
    uint8_t *o;
    if (samp_pool(POOL_OBJECT))
        for (id = 0; id < object_max() && found < 0; id++)
            if ((o = object(id)) != NULL && AT(o, ENTITY_HANDLE, int) == h)
                found = id;
    store1(s, found);
    script_cond(s, found >= 0);
    return 0;
}

/* 0B51 samp %2d% = pickup_handle_by_id %1d% */
static int8_t __stdcall op_0B51(Script *s)
{
    int id = script_int(s), h = 0;
    uint8_t *pool = samp_pool(POOL_PICKUP);
    if (pool && id >= 0 && id < PICKUPS)
        h = AT(pool, PICKUP_HANDLES + id * 4, int);
    store1(s, h ? h : -1);
    script_cond(s, h != 0);
    return 0;
}

/* 0B53 samp %2d% = pickup_id_by_handle %1d% */
static int8_t __stdcall op_0B53(Script *s)
{
    int h = script_int(s), id, found = -1;
    uint8_t *pool = samp_pool(POOL_PICKUP);
    if (pool && h)
        for (id = 0; id < PICKUPS && found < 0; id++)
            if (AT(pool, PICKUP_HANDLES + id * 4, int) == h)
                found = id;
    store1(s, found);
    script_cond(s, found >= 0);
    return 0;
}

/* ---- dialog, chat, cursor, game state ---------------------------------- */

/* 0BD7 samp get_dialog_text %1d%, 0BD8 ... caption: SA-MP's own strings */
static int8_t __stdcall op_0BD7(Script *s)
{
    static char empty[1];
    uint8_t *d = b() ? samp_global(b()->dialog) : NULL;
    store_ptr(s, d && AT(d, DLG_TEXT, char *) ? AT(d, DLG_TEXT, char *) : empty);
    return 0;
}

static int8_t __stdcall op_0BD8(Script *s)
{
    static char empty[1];
    uint8_t *d = b() ? samp_global(b()->dialog) : NULL;
    store_ptr(s, d ? (char *)d + DLG_CAPTION : empty);
    return 0;
}

static uint8_t *dialog_listbox(void)
{
    uint8_t *d = b() ? samp_global(b()->dialog) : NULL;
    return d ? AT(d, DLG_LISTBOX, uint8_t *) : NULL;
}

/* 0B54 samp %1d% = listbox_items_count */
static int8_t __stdcall op_0B54(Script *s)
{
    uint8_t *lb = dialog_listbox();
    store1(s, lb ? AT(lb, LB_COUNT, int) : 0);
    return 0;
}

/* 0B5B samp get_listbox_item %1d% text_to %2d%: into MoonLoader's buffer */
static int8_t __stdcall op_0B5B(Script *s)
{
    int i = script_int(s);
    char *buf = (char *)(uintptr_t)script_int(s);
    uint8_t *lb = dialog_listbox();
    const char *text = NULL;
    if (lb && i >= 0 && i < AT(lb, LB_COUNT, int) && AT(lb, LB_ITEMS, char **))
        text = AT(lb, LB_ITEMS, char **)[i];
    if (buf)
        copy_string(buf, text ? text : "", ML_BUFFER);
    return 0;
}

static void *chat_editbox(void)
{
    uint8_t *in = b() ? samp_global(b()->input) : NULL;
    return in ? AT(in, INPUT_EDITBOX, void *) : NULL;
}

/* 0B76 samp set_chat_input_text %1d% */
static int8_t __stdcall op_0B76(Script *s)
{
    static char buf[129];
    void *eb;
    copy_string(buf, script_string(s, buf, sizeof buf), sizeof buf);
    if ((eb = chat_editbox()) != NULL)
        ((SetTextFn)fn(b()->editbox_set))(eb, buf, 0);
    return 0;
}

/* 0B77 samp get_chat_input_text_to %1d% */
static int8_t __stdcall op_0B77(Script *s)
{
    static char buf[129];
    void *eb = chat_editbox();
    copy_string(buf, eb ? ((GetTextFn)fn(b()->editbox_get))(eb) : "", sizeof buf);
    store_ptr(s, buf);
    return 0;
}

static uint8_t *chat(void) { return b() ? samp_global(b()->chat) : NULL; }

/* 0BDD samp set_chat_display_mode %1d%: 0 off, 1 no shadow, 2 full */
static int8_t __stdcall op_0BDD(Script *s)
{
    int mode = script_int(s);
    uint8_t *c = chat();
    if (c && mode >= 0 && mode <= 2) {
        AT(c, CHAT_MODE, int) = mode;
        AT(c, CHAT_REDRAW, int) = 1;
    }
    return 0;
}

/* 0B74 samp set_chat_string %1d% text %2d% prefix %3d% color %4d% prefix_color %5d% */
static int8_t __stdcall op_0B74(Script *s)
{
    char text[144], prefix[28];
    int id = script_int(s);
    uint32_t color, pcolor;
    uint8_t *c = chat(), *e;
    copy_string(text, script_string(s, text, sizeof text), sizeof text);
    copy_string(prefix, script_string(s, prefix, sizeof prefix), sizeof prefix);
    color = (uint32_t)script_int(s);
    pcolor = (uint32_t)script_int(s);
    if (!c || id < 0 || id >= 100)
        return 0;
    e = c + CHAT_ENTRIES + id * CHAT_ENTRY_SIZE;
    copy_string((char *)e + 0x20, text, 144);
    copy_string((char *)e + 0x04, prefix, 28);
    AT(e, 0xF4, uint32_t) = color;
    AT(e, 0xF8, uint32_t) = pcolor;
    AT(c, CHAT_REDRAW, int) = 1;
    return 0;
}

/* 0B5D samp toggle_cursor %1d%: SetCursorMode(3 or 0), and when hiding it,
 * SA-MP's input re-enabling, as SAMPFUNCS does */
static int8_t __stdcall op_0B5D(Script *s)
{
    samp_set_cursor_mode(script_int(s) ? CURSOR_LOCKCAM : 0);
    return 0;
}

/* 0B27 samp set_gamestate %1d%, numbered as sampGetGamestate returns it */
static int8_t __stdcall op_0B27(Script *s)
{
    static const int8_t from_sf[5] = { -1, GS_WAITING, GS_AWAITING_JOIN, GS_CONNECTED, GS_RESTARTING };
    int st = script_int(s);
    void *ng = samp_netgame();
    if (ng && st > 0 && st < 5)
        AT(ng, b()->ng_state, int) = b()->state[from_sf[st]];
    return 0;
}

/* ---- the local player, chat input, scoreboard, send rates --------------- */

typedef void (THISCALL *ThisFn)(void *);

/* 0AF6 samp spawn_player */
static int8_t __stdcall op_0AF6(Script *s)
{
    void *lp = samp_local_player();
    (void)s;
    if (lp)
        ((ThisFn)fn(b()->local_spawn))(lp);
    return 0;
}

/* 0AFD samp set_special_action %1d% */
static int8_t __stdcall op_0AFD(Script *s)
{
    int action = script_int(s);
    void *lp = samp_local_player();
    if (lp)
        ((void (THISCALL *)(void *, int))fn(b()->local_set_special))(lp, action & 0xFF);
    return 0;
}

/* 0B79 samp set_chat_input_enabled %1d%: CInput::Open or Close */
static int8_t __stdcall op_0B79(Script *s)
{
    int on = script_int(s) != 0;
    void *in = b() ? samp_global(b()->input) : NULL;
    if (in)
        ((ThisFn)fn(on ? b()->input_open : b()->input_close))(in);
    return 0;
}

/* 0BD6 samp enable_scoreboard %1d%: CScoreboard::Enable, or Close hiding the cursor */
static int8_t __stdcall op_0BD6(Script *s)
{
    int on = script_int(s) != 0;
    void *sb = b() ? samp_global(b()->scoreboard) : NULL;
    if (!sb)
        return 0;
    if (on)
        ((ThisFn)fn(b()->scoreboard_enable))(sb);
    else
        ((void (THISCALL *)(void *, int))fn(b()->scoreboard_close))(sb, 1);
    return 0;
}

/* 0B22 samp set_sendrate %1d% to %2d%: 1 on foot, 2 in car, 3 aim */
static int8_t __stdcall op_0B22(Script *s)
{
    script_collect(s, 2);
    if (b() && ScriptParams[0].i >= 1 && ScriptParams[0].i <= 3)
        AT(samp_base, b()->sendrates + (ScriptParams[0].i - 1) * 4, int32_t) = ScriptParams[1].i;
    return 0;
}

/* ---- CLEO's script opcodes, when CLEO is not there --------------------- */

#define ACTIVE_SCRIPTS (*(Script **)0xA8B42C)   /* CTheScripts::pActiveScripts */

/* 0AAA %2d% = get_script_struct_named %1d% */
static int8_t __stdcall op_0AAA(Script *s)
{
    char name[16];
    Script *t;
    copy_string(name, script_string(s, name, sizeof name), 9);
    for (t = ACTIVE_SCRIPTS; t; t = t->next) {
        char own[9];
        memcpy(own, t->name, 8);
        own[8] = 0;
        if (lstrcmpiA(own, name) == 0)
            break;
    }
    store_ptr(s, t);
    script_cond(s, t != NULL);
    return 0;
}

static int32_t shared[1024];

/* 0AB3 cleo_shared_var %1d% = %2d% */
static int8_t __stdcall op_0AB3(Script *s)
{
    script_collect(s, 2);
    if ((unsigned)ScriptParams[0].i < 1024)
        shared[ScriptParams[0].i] = ScriptParams[1].i;
    return 0;
}

/* 0AB4 %2d% = cleo_shared_var %1d% */
static int8_t __stdcall op_0AB4(Script *s)
{
    int i = script_int(s);
    store1(s, (unsigned)i < 1024 ? shared[i] : 0);
    return 0;
}

/* 0A92 create_custom_thread %1d%, 0A94 start_custom_mission %1d%: CLEO
 * scripts need CLEO; false, and their arguments skipped */
static int8_t __stdcall op_no_cleo(Script *s)
{
    char buf[256];
    script_string(s, buf, sizeof buf);
    script_skip_varargs(s);
    script_cond(s, 0);
    return 0;
}

void info_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0AF7, op_0AF7 }, { 0x0B7F, op_0B7F }, { 0x0BAE, op_0BAE }, { 0x0BAF, op_0BAF },
        { 0x0BB0, op_0BB0 }, { 0x0BB1, op_0BB1 }, { 0x0BB2, op_0BB2 }, { 0x0BAC, op_0BAC },
        { 0x0BAD, op_0BAD }, { 0x0BB3, op_0BB3 }, { 0x0BB4, op_0BB4 }, { 0x0BB6, op_0BB6 },
        { 0x0BB7, op_0BB7 }, { 0x0BB8, op_0BB8 }, { 0x0BB9, op_0BB9 }, { 0x0B7A, op_0B7A },
        { 0x0B7C, op_0B7C }, { 0x0B24, op_0B24 }, { 0x0B50, op_0B50 }, { 0x0B52, op_0B52 },
        { 0x0B51, op_0B51 }, { 0x0B53, op_0B53 }, { 0x0BD7, op_0BD7 }, { 0x0BD8, op_0BD8 },
        { 0x0B76, op_0B76 }, { 0x0B77, op_0B77 }, { 0x0BDD, op_0BDD }, { 0x0B74, op_0B74 },
        { 0x0B5D, op_0B5D }, { 0x0B27, op_0B27 }, { 0x0AF6, op_0AF6 }, { 0x0AFD, op_0AFD },
        { 0x0B79, op_0B79 }, { 0x0BD6, op_0BD6 }, { 0x0B22, op_0B22 }, { 0x0B7B, op_0B7B },
        { 0x0B7D, op_0B7D }, { 0x0B7E, op_0B7E }, { 0x0B54, op_0B54 }, { 0x0B5B, op_0B5B },
    };
    static const struct { uint16_t op; OpcodeFn fn; } cleo_ops[] = {
        { 0x0AAA, op_0AAA }, { 0x0AB3, op_0AB3 }, { 0x0AB4, op_0AB4 },
        { 0x0A92, op_no_cleo }, { 0x0A94, op_no_cleo },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
    if (!script_cleo_present())
        for (i = 0; i < sizeof cleo_ops / sizeof cleo_ops[0]; i++)
            script_register(cleo_ops[i].op, cleo_ops[i].fn);
}
