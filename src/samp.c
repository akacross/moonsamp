/* SA-MP side: SAMPFUNCS's opcodes, implemented against samp.dll.
 *
 * Builds reorder their structs, so each one gets its own table. Offsets come
 * from SAMP-API (R3-1) and from tools/find_offsets.py and the build's own
 * code (0.3.DL). */
#include "moonsamp.h"

static const Build builds[] = {
    { "0.3.7-R3-1", 0xCC4D0, 0x26E8DC, 0x26E8C8, 0x26E8CC,
      0x679F0, 0x5820, 0x69190, 0xA6AF0,
      0x0000, 0x0FB4, 0x0004, 0x2F1C, 0x2F22, 0x2F3A,
      0x00, 0x0C, 0x00,
      0x408E7, 0x3A6A0, 0x121914, 0x53A60, 0x53B90, 0x1EFF0, 0x1F190,
      0x26E898, 0x26E8F4, 0x26E894, 0x6F8C0, 0x6FF40, 0x88760, 0x8A2E0, 0x84F40, 0x84E70,
      0xA1E0, 0x9FFE0, 0xB5C0, 0x1E440, 0x3340, 0xB26D0, 0x04, 0x2F14 },
    { "0.3.DL-R1", 0xFDB60, 0x2ACA24, 0x2ACA10, 0x2ACA14,
      0x67BE0, 0x5860, 0x69340, 0xA6F70,
      0x0022, 0x0FD6, 0x0026, 0x0000, 0x0006, 0x001E,
      0x08, 0x14, 0x04,
      0x40AE7, 0x3A8A0, 0x15FA54, 0x53C60, 0x53D90, 0x1F1F0, 0x1F390,
      0x2AC9E0, 0x2ACA3C, 0x2AC9DC, 0x6FA50, 0x700D0, 0x888F0, 0x8A470, 0x850D0, 0x85000,
      0xA230, 0xA0530, 0xB5E0, 0x1E650, 0x3390, 0xB2B60, 0x0C, 0x2F36 },
};

/* Shared by every supported build. */
#define NETGAME_STATE   0x3CD
#define NETGAME_POOLS   0x3DE
#define SAMPPED_GAMEPED 0x2A4
#define INPUT_PROCS     0x00C
#define INPUT_NAMES     0x24C
#define INPUT_COUNT     0x14DC
#define INPUT_ENABLED   0x14E0
#define MAX_COMMANDS    144
#define MAX_PLAYERS     1004

typedef void *(__cdecl *GetPedFn)(int);
typedef int (__cdecl *GetPedRefFn)(void *);
typedef void *(__cdecl *FindPlayerPedFn)(int);
#define CPools_GetPed    ((GetPedFn)0x54FF90)
#define CPools_GetPedRef ((GetPedRefFn)0x54FF60)
#define FindPlayerPed    ((FindPlayerPedFn)0x56E210)

typedef void (THISCALL *TextFn)(void *, const char *);
typedef void (THISCALL *AddMessageFn)(void *, uint32_t, const char *);
typedef uint32_t (__stdcall *ColorFn)(int);
typedef void (__cdecl *CmdProc)(const char *);

uintptr_t samp_base;
#define base samp_base
static const Build *B;
static int probed;

const Build *samp_build(void)
{
    HMODULE m;
    if (B)
        return B;
    m = GetModuleHandleA("samp.dll");
    if (!m)
        return NULL;
    if (!probed) {
        uint32_t entry;
        size_t i;
        probed = 1;
        base = (uintptr_t)m;
        entry = AT(base + AT(base, 0x3C, uint32_t), 0x28, uint32_t);
        for (i = 0; i < sizeof builds / sizeof builds[0]; i++)
            if (builds[i].entry == entry)
                B = &builds[i];
        if (B)
            ms_log("samp.dll %s at %08lX", B->name, (DWORD)base);
        else
            ms_log("samp.dll build not supported (entry point 0x%lX)", (DWORD)entry);
    }
    return B;
}

#define build samp_build

void *samp_global(uint32_t rva) { return build() ? *(void **)(base + rva) : NULL; }
#define global samp_global
void *samp_netgame(void) { return build() ? global(B->netgame) : NULL; }
#define netgame samp_netgame

void *samp_pool(int slot)
{
    void *ng = netgame(), *pools;
    if (!ng)
        return NULL;
    pools = AT(ng, NETGAME_POOLS, void *);
    return pools ? AT(pools, slot, void *) : NULL;
}

static void *player_pool(void) { return samp_pool(POOL_PLAYER); }

void copy_string(char *dst, const char *src, size_t size)
{
    if (dst && size)
        lstrcpynA(dst, src ? src : "", (int)size);
}

static void *input(void) { return build() ? global(B->input) : NULL; }
static void *chat(void)  { return build() ? global(B->chat) : NULL; }

static const char *msvc_string(void *s)
{
    return AT(s, 20, uint32_t) >= 16 ? AT(s, 0, const char *) : (const char *)s;
}

static int local_id(void *pp) { return AT(pp, B->pp_local_id, uint16_t); }

static void *player_info(void *pp, int id)
{
    if (id < 0 || id >= MAX_PLAYERS || id > AT(pp, B->pp_largest, int))
        return NULL;
    if (AT(pp, B->pp_used + id * 4, int) != 1)
        return NULL;
    return AT(pp, B->pp_info + id * 4, void *);
}

static void *remote_game_ped(void *pp, int id)
{
    void *info = player_info(pp, id), *remote, *ped;
    if (!info || !(remote = AT(info, B->info_remote, void *)))
        return NULL;
    if (!(ped = AT(remote, B->remote_ped, void *)))
        return NULL;
    return AT(ped, SAMPPED_GAMEPED, void *);
}

/* ---- client commands ------------------------------------------------ */

typedef struct {
    char name[33];
    Script *script;
    int32_t label;
    int used, added;
} Command;

static Command commands[MAX_COMMANDS];
static uint8_t *thunks;
static char last_params[256];

static int name_eq(const char *a, const char *b) { return lstrcmpiA(a, b) == 0; }

static int input_find(void *in, const char *name)
{
    int n = AT(in, INPUT_COUNT, int), i;
    for (i = 0; i < n; i++)
        if (name_eq((char *)in + INPUT_NAMES + i * 33, name))
            return i;
    return -1;
}

static int input_add(void *in, const char *name, void *proc)
{
    int n = AT(in, INPUT_COUNT, int);
    if (n >= MAX_COMMANDS || input_find(in, name) >= 0)
        return 0;
    AT(in, INPUT_PROCS + n * 4, void *) = proc;
    lstrcpynA((char *)in + INPUT_NAMES + n * 33, name, 33);
    AT(in, INPUT_COUNT, int) = n + 1;
    return 1;
}

static void input_remove(void *in, const char *name)
{
    int n = AT(in, INPUT_COUNT, int), i = input_find(in, name);
    if (i < 0)
        return;
    for (; i < n - 1; i++) {
        AT(in, INPUT_PROCS + i * 4, void *) = AT(in, INPUT_PROCS + (i + 1) * 4, void *);
        memcpy((char *)in + INPUT_NAMES + i * 33, (char *)in + INPUT_NAMES + (i + 1) * 33, 33);
    }
    AT(in, INPUT_COUNT, int) = n - 1;
}

/* Commands registered before SA-MP's input box exists are added once it does. */
static void flush_commands(void)
{
    void *in = input();
    int i;
    if (!in)
        return;
    for (i = 0; i < MAX_COMMANDS; i++)
        if (commands[i].used && !commands[i].added) {
            commands[i].added = 1;
            if (!input_add(in, commands[i].name, thunks + i * 10))
                ms_log("could not add /%s to SA-MP", commands[i].name);
            else if (script_trace)
                ms_log("trace: /%s added to SA-MP (%d commands)", commands[i].name, AT(in, INPUT_COUNT, int));
        }
}

/* Runs a command's callback stub; 0B35 hands it params. */
void samp_run_command(Script *s, int32_t label, const char *params)
{
    lstrcpynA(last_params, params ? params : "", sizeof last_params);
    script_run_label(s, label);
}

void __cdecl on_command(int i, const char *params)
{
    Command *c = &commands[i];
    if (script_trace)
        ms_log("trace: SA-MP runs /%s \"%s\"%s", c->name, params ? params : "", c->used ? "" : " (unused slot)");
    if (c->used)
        samp_run_command(c->script, c->label, params);
}

/* SA-MP calls a command as cdecl proc(params). Each command gets a stub
 * `mov eax, index; jmp command_entry`. */
__asm__(
    ".text\n"
    ".globl _command_entry\n"
    "_command_entry:\n\t"
    "pushl 4(%esp)\n\t"
    "pushl %eax\n\t"
    "call _on_command\n\t"
    "addl $8, %esp\n\t"
    "ret\n");
extern void command_entry(void);

static int make_thunks(void)
{
    int i;
    thunks = VirtualAlloc(NULL, MAX_COMMANDS * 10, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!thunks)
        return 0;
    for (i = 0; i < MAX_COMMANDS; i++) {
        uint8_t *t = thunks + i * 10;
        t[0] = 0xB8;
        *(int32_t *)(t + 1) = i;
        t[5] = 0xE9;
        *(int32_t *)(t + 6) = (int32_t)((uint8_t *)command_entry - (t + 10));
    }
    return 1;
}

static Command *command_by_name(const char *name)
{
    int i;
    for (i = 0; i < MAX_COMMANDS; i++)
        if (commands[i].used && name_eq(commands[i].name, name))
            return &commands[i];
    return NULL;
}

/* ---- opcodes --------------------------------------------------------- */

static void ready(void)
{
    if (build())
        flush_commands();
}

/* Traced: SA-MP's own state behind the checks scripts make, logged when it
 * changes. */
static void trace_state(void)
{
    static int last[7] = { -9, -9, -9, -9, -9, -9, -9 };
    void *in = input(), *dlg = global(B->dialog), *sb = global(B->scoreboard);
    void *g = global(B->game), *lp = samp_local_player(), *ng = netgame();
    int now[7], i, changed = 0;
    now[0] = in ? AT(in, INPUT_ENABLED, int) : -1;
    now[1] = dlg ? AT(dlg, 0x28, int) : -1;
    now[2] = sb ? AT(sb, 0, int) : -1;
    now[3] = g ? AT(g, 0x61, int) : -1;
    now[4] = lp ? AT(lp, 0xF4, int) : -1;
    now[5] = lp ? AT(lp, 0xF8, int) : -1;
    now[6] = ng ? AT(ng, NETGAME_STATE, int) : -1;
    for (i = 0; i < 7; i++)
        changed |= now[i] != last[i];
    if (!changed)
        return;
    for (i = 0; i < 7; i++)
        last[i] = now[i];
    ms_log("state: chat input %d, dialog %d, scoreboard %d, cursor %d, local +F4 %d +F8 %d, gamestate %d",
           now[0], now[1], now[2], now[3], now[4], now[5], now[6]);
}

/* Traced: keyboard messages reaching the game thread's queue (a
 * WH_GETMESSAGE hook, before any window procedure), which window has focus,
 * and which module heads GTA's window procedure chain, once a second. */
static volatile LONG keydowns, syskeys, chars;
static HHOOK msg_hook;

static void key_name(char *out, WPARAM vk)
{
    switch (vk) {
    case 'T': lstrcpyA(out, "T"); break;
    case VK_TAB: lstrcpyA(out, "Tab"); break;
    case VK_ESCAPE: lstrcpyA(out, "Esc"); break;
    case VK_F6: lstrcpyA(out, "F6"); break;
    case VK_MENU: lstrcpyA(out, "Alt"); break;
    case VK_RETURN: lstrcpyA(out, "Enter"); break;
    default: out[0] = 0;
    }
}

static LRESULT CALLBACK msg_proc(int code, WPARAM wp, LPARAM lp)
{
    if (code >= 0 && wp == PM_REMOVE) {
        MSG *m = (MSG *)lp;
        char name[8];
        if (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN) {
            if (m->message == WM_KEYDOWN)
                InterlockedIncrement(&keydowns);
            else
                InterlockedIncrement(&syskeys);
            key_name(name, m->wParam);
            if (name[0] && !(m->lParam & 0x40000000))
                ms_log("input: %s %s to window %08lX", m->message == WM_SYSKEYDOWN ? "syskey" : "key",
                       name, (DWORD)m->hwnd);
        } else if (m->message == WM_CHAR) {
            InterlockedIncrement(&chars);
        }
    }
    return CallNextHookEx(msg_hook, code, wp, lp);
}

static void trace_input(void)
{
    static DWORD next, last_focus, last_proc;
    static LONG last_keys = -1;
    GUITHREADINFO gi;
    DWORD now = GetTickCount(), proc = 0;
    HWND game;
    char mod[MAX_PATH] = "?";
    HMODULE m;
    if (!msg_hook) {
        msg_hook = SetWindowsHookExA(WH_GETMESSAGE, msg_proc, NULL, GetCurrentThreadId());
        ms_log("input: message hook %s", msg_hook ? "on" : "failed");
    }
    if ((int)(now - next) < 0)
        return;
    next = now + 1000;
    gi.cbSize = sizeof gi;
    if (!GetGUIThreadInfo(GetCurrentThreadId(), &gi))
        memset(&gi, 0, sizeof gi);
    {
        HWND *ps = *(HWND **)(0xC17040 + 0x14);            /* RsGlobal.ps; ->window first */
        game = ps ? *ps : NULL;
    }
    if (game)
        proc = (DWORD)GetWindowLongA(game, GWL_WNDPROC);
    if (proc && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)proc, &m)) {
        char *slash;
        GetModuleFileNameA(m, mod, sizeof mod);
        slash = mod + lstrlenA(mod);
        while (slash > mod && slash[-1] != '\\' && slash[-1] != '/')
            slash--;
        lstrcpyA(mod, slash);
    }
    if (keydowns + syskeys + chars == last_keys && (DWORD)gi.hwndFocus == last_focus && proc == last_proc)
        return;
    last_keys = keydowns + syskeys + chars;
    last_focus = (DWORD)gi.hwndFocus;
    last_proc = proc;
    ms_log("input: totals keydown %ld syskey %ld char %ld | game window %08lX focus %08lX active %08lX foreground %s | window proc %08lX in %s",
           keydowns, syskeys, chars, (DWORD)game, (DWORD)gi.hwndFocus, (DWORD)gi.hwndActive,
           GetForegroundWindow() == game ? "game" : "other", proc, mod);
}

/* Once a frame, from the CTimer::Update hook. */
void samp_tick(void)
{
    if (!build())
        return;
    flush_commands();
    raknet_tick();
    if (script_trace) {
        trace_state();
        trace_input();
    }
}

/* 0AFA is_samp_available */
static int8_t __stdcall op_0AFA(Script *s)
{
    ready();
    script_cond(s, player_pool() && chat() && input());
    return 0;
}

/* 0B4F samp %1d% = get_gamestate */
static int8_t __stdcall op_0B4F(Script *s)
{
    void *ng = netgame();
    int st = 0;
    if (ng)
        switch (AT(ng, NETGAME_STATE, int)) {
        case 1: case 2: st = 1; break;   /* waiting / connecting */
        case 6: st = 2; break;           /* awaiting join */
        case 5: st = 3; break;           /* connected */
        case 11: st = 4; break;          /* restarting */
        }
    ScriptParams[0].i = st;
    script_store(s, 1);
    return 0;
}

/* 0AF8 samp add_message_to_chat %1s% color %2d% ... */
static int8_t __stdcall op_0AF8(Script *s)
{
    char buf[144];
    const char *text;
    uint32_t color;
    void *c;
    text = script_string(s, buf, sizeof buf);
    color = (uint32_t)script_int(s);
    script_skip_varargs(s);
    if ((c = chat()) != NULL) {
        if (text != buf) {
            lstrcpynA(buf, text, sizeof buf);
            text = buf;
        }
        ((AddMessageFn)(base + B->add_message))(c, color, text);
        if (script_trace)
            ms_log("trace: chat message %08lX \"%.80s\"", (DWORD)color, text);
    } else if (script_trace) {
        ms_log("trace: chat message dropped, no CChat: \"%.80s\"", text);
    }
    return 0;
}

void *samp_local_player(void)
{
    void *pp = player_pool();
    return pp ? AT(pp, B->pp_local_obj, void *) : NULL;
}
#define local_player samp_local_player

static void say(const char *text)
{
    void *lp;
    if (text[0] == '/') {
        if (input())
            ((TextFn)(base + B->send_command))(input(), text);
    } else if ((lp = local_player()) != NULL) {
        ((TextFn)(base + B->local_chat))(lp, text);
    }
}

/* 0AF9 samp say_msg %1s% ... : a leading '/' goes to the server as a
 * command, skipping client commands, as SAMPFUNCS does. */
static int8_t __stdcall op_0AF9(Script *s)
{
    char buf[144];
    const char *text = script_string(s, buf, sizeof buf);
    script_skip_varargs(s);
    if (text != buf) {
        lstrcpynA(buf, text, sizeof buf);
        text = buf;
    }
    if (build())
        say(text);
    return 0;
}

/* 0C8F samp process_chat_input %1d%: as if typed into the chat box. */
static int8_t __stdcall op_0C8F(Script *s)
{
    char buf[144], name[33];
    const char *text = script_string(s, buf, sizeof buf), *args;
    void *in;
    int i, n;
    if (text != buf) {
        lstrcpynA(buf, text, sizeof buf);
        text = buf;
    }
    ready();
    if (!(in = input()))
        return 0;
    if (text[0] != '/') {
        say(text);
        return 0;
    }
    for (n = 0; text[1 + n] && text[1 + n] != ' ' && n < 32; n++)
        name[n] = text[1 + n];
    name[n] = 0;
    args = text + 1 + n;
    if (*args == ' ')
        args++;
    i = input_find(in, name);
    if (i >= 0)
        AT(in, INPUT_PROCS + i * 4, CmdProc)(args);
    else
        say(text);
    return 0;
}

/* 0B34 samp register_client_command %1s% to_label %2p% */
static int8_t __stdcall op_0B34(Script *s)
{
    char name[64];
    int32_t label;
    int i;
    script_string(s, name, sizeof name);
    label = script_int(s);
    ready();
    if (!name[0] || lstrlenA(name) > 32 || command_by_name(name) ||
        (input() && input_find(input(), name) >= 0)) {
        script_cond(s, 0);
        return 0;
    }
    for (i = 0; i < MAX_COMMANDS && commands[i].used; i++)
        ;
    if (i == MAX_COMMANDS) {
        script_cond(s, 0);
        return 0;
    }
    lstrcpynA(commands[i].name, name, sizeof commands[i].name);
    commands[i].script = s;
    commands[i].label = label;
    commands[i].added = 0;
    commands[i].used = 1;
    flush_commands();
    script_cond(s, 1);
    return 0;
}

/* 0B63 samp unregister_client_command %1d% */
static int8_t __stdcall op_0B63(Script *s)
{
    char name[64];
    Command *c;
    script_string(s, name, sizeof name);
    c = command_by_name(name);
    if (c) {
        if (c->added && input())
            input_remove(input(), c->name);
        c->used = c->added = 0;
    }
    script_cond(s, c != NULL);
    return 0;
}

/* 0C90 samp is_client_command_defined %1d% */
static int8_t __stdcall op_0C90(Script *s)
{
    char name[64];
    script_string(s, name, sizeof name);
    ready();
    script_cond(s, command_by_name(name) || (input() && input_find(input(), name) >= 0));
    return 0;
}

/* 0B35 samp %1d% = get_last_command_params */
static int8_t __stdcall op_0B35(Script *s)
{
    ScriptParams[0].p = last_params;
    script_store(s, 1);
    return 0;
}

/* 0B43 samp cmd_ret */
static int8_t __stdcall op_0B43(Script *s)
{
    (void)s;
    script_label_return();
    return 0;
}

/* 0B21 samp is_chat_opened */
static int8_t __stdcall op_0B21(Script *s)
{
    void *in = input();
    script_cond(s, in && AT(in, INPUT_ENABLED, int));
    return 0;
}

/* 0B20 samp %2d% = actor_handle_by_samp_player_id %1d% */
static int8_t __stdcall op_0B20(Script *s)
{
    int id = script_int(s);
    void *pp = player_pool(), *ped = NULL;
    if (pp)
        ped = id == local_id(pp) ? FindPlayerPed(-1) : remote_game_ped(pp, id);
    ScriptParams[0].i = ped ? CPools_GetPedRef(ped) : -1;
    script_store(s, 1);
    script_cond(s, ped != NULL);
    return 0;
}

/* 0B2B samp %2d% = get_player_id_by_actor_handle %1d% */
static int8_t __stdcall op_0B2B(Script *s)
{
    int handle = script_int(s), id = -1, i, top;
    void *pp = player_pool(), *ped = CPools_GetPed(handle);
    if (pp && ped) {
        if (ped == FindPlayerPed(-1)) {
            id = local_id(pp);
        } else {
            top = AT(pp, B->pp_largest, int);
            for (i = 0; i <= top && i < MAX_PLAYERS; i++)
                if (remote_game_ped(pp, i) == ped) {
                    id = i;
                    break;
                }
        }
    }
    ScriptParams[0].i = id;
    script_store(s, 1);
    script_cond(s, id >= 0);
    return 0;
}

/* 0B23 samp is_player_connected %1d% */
static int8_t __stdcall op_0B23(Script *s)
{
    int id = script_int(s);
    void *pp = player_pool();
    script_cond(s, pp && player_info(pp, id) != NULL);
    return 0;
}

/* 0B36 samp %2d% = get_player_nickname %1d% */
static int8_t __stdcall op_0B36(Script *s)
{
    static char empty[1];
    int id = script_int(s);
    void *pp = player_pool(), *info;
    const char *name = empty;
    if (pp) {
        if (id == local_id(pp))
            name = msvc_string((uint8_t *)pp + B->pp_local_name);
        else if ((info = player_info(pp, id)) != NULL)
            name = msvc_string((uint8_t *)info + B->info_name);
    }
    ScriptParams[0].p = (void *)name;
    script_store(s, 1);
    script_cond(s, name != empty);
    return 0;
}

/* 0B37 samp %2d% = get_player_color %1d%: ARGB with the alpha kept, which
 * servers set to 0 to hide a player's marker. */
static int8_t __stdcall op_0B37(Script *s)
{
    int id = script_int(s);
    uint32_t c = 0;
    if (build() && id >= 0 && id < MAX_PLAYERS) {
        uint32_t rgba = ((ColorFn)(base + B->player_color))(id);
        c = rgba >> 8 | rgba << 24;
    }
    ScriptParams[0].u = c;
    script_store(s, 1);
    return 0;
}

/* ---- more player state (phase 2) ----------------------------------- */

#define REMOTE_SPECIAL_ACTION 0x18
#define REMOTE_ARMOUR         0x1AC
#define REMOTE_HEALTH         0x1B0
#define REMOTE_STATUS         0x1D1     /* 2: no sync for 1.5-3 s (paused) */
#define LOCAL_ACTIVE          0xF4      /* set by CLocalPlayer::Spawn */
#define GTA_PED_HEALTH        0x540
#define GTA_PED_ARMOUR        0x548
#define GTA_ANIMATIONS        0xB4EA40  /* CAnimManager::ms_aAnimations */
#define GTA_ANIMATION_COUNT   2500

typedef void *(__cdecl *GetVehicleFn)(int);
typedef void *(__cdecl *FirstAssocFn)(void *);
#define CPools_GetVehicle   ((GetVehicleFn)0x54FFF0)
#define FirstAssociation    ((FirstAssocFn)0x4D15E0)

static void *remote_player(void *pp, int id)
{
    void *info = player_info(pp, id);
    return info ? AT(info, B->info_remote, void *) : NULL;
}

static void *player_game_ped(void *pp, int id)
{
    return id == local_id(pp) ? FindPlayerPed(-1) : remote_game_ped(pp, id);
}

static void store1(Script *s, int32_t v)
{
    ScriptParams[0].i = v;
    script_store(s, 1);
}

/* 0B61 samp is_local_player_spawned */
static int8_t __stdcall op_0B61(Script *s)
{
    void *lp = local_player();
    script_cond(s, lp && AT(lp, LOCAL_ACTIVE, int));
    return 0;
}

/* 0C8A samp %2d% = get_max_player_id streamed_only %1d% */
static int8_t __stdcall op_0C8A(Script *s)
{
    int streamed = script_int(s), top = -1, i, max;
    void *pp = player_pool();
    if (pp) {
        max = AT(pp, B->pp_largest, int);
        for (i = 0; i <= max && i < MAX_PLAYERS; i++)
            if (player_info(pp, i) && (!streamed || remote_game_ped(pp, i)))
                top = i;
        if (local_id(pp) > top)
            top = local_id(pp);
    }
    store1(s, top);
    return 0;
}

/* 0C8B samp %2d% = get_player_count streamed_only %1d% (the local player included) */
static int8_t __stdcall op_0C8B(Script *s)
{
    int streamed = script_int(s), n = 0, i, max;
    void *pp = player_pool();
    if (pp) {
        n = 1;
        max = AT(pp, B->pp_largest, int);
        for (i = 0; i <= max && i < MAX_PLAYERS; i++)
            if (player_info(pp, i) && (!streamed || remote_game_ped(pp, i)))
                n++;
    }
    store1(s, n);
    return 0;
}

/* 0B57 samp %2d% = player %1d% animation_id: the ped's strongest-blended
 * animation, numbered as GTA's (and SA-MP's) animation table. */
static int8_t __stdcall op_0B57(Script *s)
{
    int id = script_int(s), best = -1;
    void *pp = player_pool(), *ped, *clump;
    float top = -1.0f;
    if (pp && (ped = player_game_ped(pp, id)) && (clump = AT(ped, 0x18, void *))) {
        uint8_t *a = FirstAssociation(clump);
        int guard = 0;
        while (a && guard++ < 64) {
            uintptr_t h = AT(a, 0x14, uintptr_t);
            float blend = AT(a, 0x18, float);
            if (h >= GTA_ANIMATIONS && h < GTA_ANIMATIONS + GTA_ANIMATION_COUNT * 0x18 && blend > top) {
                top = blend;
                best = (int)((h - GTA_ANIMATIONS) / 0x18);
            }
            a = AT(a, 4, uint8_t *);
            a = a ? a - 4 : NULL;
        }
    }
    store1(s, best < 0 ? 0 : best);
    script_cond(s, best >= 0);
    return 0;
}

/* 0B5C samp is_player %1d% paused */
static int8_t __stdcall op_0B5C(Script *s)
{
    int id = script_int(s);
    void *pp = player_pool(), *rp;
    script_cond(s, pp && id != local_id(pp) && (rp = remote_player(pp, id)) &&
                   AT(rp, REMOTE_STATUS, int) == 2);
    return 0;
}

/* 0B2A samp %2d% = get_player_ping %1d% */
static int8_t __stdcall op_0B2A(Script *s)
{
    int id = script_int(s), ping = 0;
    void *pp = player_pool(), *info;
    if (pp) {
        if (id == local_id(pp))
            ping = AT(pp, B->pp_local_ping, int);
        else if ((info = player_info(pp, id)) != NULL)
            ping = AT(info, B->info_ping, int);
    }
    store1(s, ping);
    return 0;
}

static int8_t health_or_armour(Script *s, int armour)
{
    int id = script_int(s);
    float v = 0;
    void *pp = player_pool(), *ped, *rp;
    if (pp) {
        if (id == local_id(pp)) {
            if ((ped = FindPlayerPed(-1)) != NULL)
                v = AT(ped, armour ? GTA_PED_ARMOUR : GTA_PED_HEALTH, float);
        } else if ((rp = remote_player(pp, id)) != NULL) {
            v = AT(rp, armour ? REMOTE_ARMOUR : REMOTE_HEALTH, float);
        }
    }
    store1(s, (int32_t)v);
    return 0;
}

/* 0B25 samp %2d% = get_player_health %1d%, 0B26 ... armor */
static int8_t __stdcall op_0B25(Script *s) { return health_or_armour(s, 0); }
static int8_t __stdcall op_0B26(Script *s) { return health_or_armour(s, 1); }

/* 0B62 samp %2d% = player %1d% special_action */
static int8_t __stdcall op_0B62(Script *s)
{
    typedef char (THISCALL *SpecialFn)(void *);
    int id = script_int(s), v = 0;
    void *pp = player_pool(), *rp, *lp;
    if (pp) {
        if (id == local_id(pp)) {
            if ((lp = local_player()) != NULL)
                v = (uint8_t)((SpecialFn)(base + B->special_action))(lp);
        } else if ((rp = remote_player(pp, id)) != NULL) {
            v = AT(rp, REMOTE_SPECIAL_ACTION, uint8_t);
        }
    }
    store1(s, v);
    return 0;
}

/* 0B2C samp %2d% = get_vehicle_id_by_car_handle %1d% */
static int8_t __stdcall op_0B2C(Script *s)
{
    typedef uint16_t (THISCALL *FindFn)(void *, void *);
    int handle = script_int(s), id = -1;
    void *vp = samp_pool(POOL_VEHICLE), *car = CPools_GetVehicle(handle);
    if (vp && car) {
        uint16_t r = ((FindFn)(base + B->vehicle_find))(vp, car);
        if (r != 0xFFFF)
            id = r;
    }
    store1(s, id);
    script_cond(s, id >= 0);
    return 0;
}

/* 0B29 samp set_local_name %1s% */
static int8_t __stdcall op_0B29(Script *s)
{
    typedef void (THISCALL *NameFn)(void *, const char *);
    char buf[32];
    const char *name = script_string(s, buf, sizeof buf);
    void *pp = player_pool();
    copy_string(buf, name, sizeof buf);
    if (pp && buf[0])
        ((NameFn)(base + B->set_local_name))(pp, buf);
    return 0;
}

/* 0B46 samp is_3d_text_defined %1d% */
static int8_t __stdcall op_0B46(Script *s)
{
    int id = script_int(s);
    void *lp = samp_pool(POOL_LABEL);
    script_cond(s, lp && id >= 0 && id < 2048 && AT(lp, 0xE800 + id * 4, int));
    return 0;
}

/* 0BB5 samp %1d% = get_gangzone_pool_ptr */
static int8_t __stdcall op_0BB5(Script *s)
{
    store1(s, (int32_t)(uintptr_t)samp_pool(POOL_GANGZONE));
    return 0;
}

/* 0BDC samp %1d% = get_chat_display_mode */
static int8_t __stdcall op_0BDC(Script *s)
{
    void *c = chat();
    store1(s, c ? AT(c, 0x8, int) : 0);
    return 0;
}

/* 0B8C samp is_cursor_active */
static int8_t __stdcall op_0B8C(Script *s)
{
    void *g = build() ? global(B->game) : NULL;
    script_cond(s, g && AT(g, 0x61, int) != 0);
    return 0;
}

/* 0B8D samp set_cursor_mode %1d% */
static int8_t __stdcall op_0B8D(Script *s)
{
    typedef void (THISCALL *CursorFn)(void *, int, int);
    int mode = script_int(s);
    void *g = build() ? global(B->game) : NULL;
    if (g)
        ((CursorFn)(base + B->set_cursor_mode))(g, mode, mode == 0);
    return 0;
}

/* 0BD5 samp is_scoreboard_opened */
static int8_t __stdcall op_0BD5(Script *s)
{
    void *sb = build() ? global(B->scoreboard) : NULL;
    script_cond(s, sb && AT(sb, 0, int) != 0);
    return 0;
}

/* 0B3A samp %1s% = get_current_server_name: into MoonLoader's buffer */
static int8_t __stdcall op_0B3A(Script *s)
{
    char *buf = (char *)(uintptr_t)script_int(s);
    void *ng = netgame();
    if (buf)
        copy_string(buf, ng ? (const char *)ng + 0x131 : "", 257);
    return 0;
}

/* 0B39 samp get_current_server_address %1d% port %2d% */
static int8_t __stdcall op_0B39(Script *s)
{
    char *buf = (char *)(uintptr_t)script_int(s);
    void *ng = netgame();
    if (buf)
        copy_string(buf, ng ? (const char *)ng + 0x30 : "", 257);
    store1(s, ng ? AT(ng, 0x235, int) : 0);
    return 0;
}

/* 0B38 samp connect_to_server %1s% port %2d%: drop the connection, reset
 * the world as for a server restart, and let SA-MP connect again. */
static int8_t __stdcall op_0B38(Script *s)
{
    typedef void (THISCALL *ResetFn)(void *);
    typedef void (THISCALL *DisconnectFn)(void *, unsigned, unsigned char);
    char ip[257];
    const char *p = script_string(s, ip, sizeof ip);
    int port = script_int(s);
    void *ng = netgame(), *rc;
    copy_string(ip, p, sizeof ip);
    if (!ng || !ip[0])
        return 0;
    if ((rc = AT(ng, 0x2C, void *)) != NULL)
        ((DisconnectFn)(*(void ***)rc)[2])(rc, 500, 0);
    ((ResetFn)(base + B->shutdown_restart))(ng);
    copy_string((char *)ng + 0x30, ip, 257);
    AT(ng, 0x235, int) = port;
    AT(ng, 0x3D1, uint32_t) = 0;        /* last connect attempt */
    AT(ng, NETGAME_STATE, int) = 1;     /* waiting to connect */
    return 0;
}

/* 0B5A get_screen_resolution %1d% %2d%: width, then height. In game,
 * MoonLoader's getScreenResolution returns them in that order. */
static int8_t __stdcall op_0B5A(Script *s)
{
    ScriptParams[0].i = *(int32_t *)0xC17044;   /* RsGlobal.maximumWidth */
    ScriptParams[1].i = *(int32_t *)0xC17048;   /* RsGlobal.maximumHeight */
    script_store(s, 2);
    return 0;
}

static void players_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B61, op_0B61 }, { 0x0C8A, op_0C8A }, { 0x0C8B, op_0C8B }, { 0x0B57, op_0B57 },
        { 0x0B5C, op_0B5C }, { 0x0B2A, op_0B2A }, { 0x0B25, op_0B25 }, { 0x0B26, op_0B26 },
        { 0x0B62, op_0B62 }, { 0x0B2C, op_0B2C }, { 0x0B29, op_0B29 }, { 0x0B46, op_0B46 },
        { 0x0BB5, op_0BB5 }, { 0x0BDC, op_0BDC }, { 0x0B8C, op_0B8C }, { 0x0B8D, op_0B8D },
        { 0x0BD5, op_0BD5 }, { 0x0B3A, op_0B3A }, { 0x0B39, op_0B39 }, { 0x0B38, op_0B38 },
        { 0x0B5A, op_0B5A },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}

int samp_install(void)
{
    if (!make_thunks())
        return 0;
    script_register(0x0AF8, op_0AF8);
    script_register(0x0AF9, op_0AF9);
    script_register(0x0AFA, op_0AFA);
    script_register(0x0B20, op_0B20);
    script_register(0x0B21, op_0B21);
    script_register(0x0B23, op_0B23);
    script_register(0x0B2B, op_0B2B);
    script_register(0x0B34, op_0B34);
    script_register(0x0B35, op_0B35);
    script_register(0x0B36, op_0B36);
    script_register(0x0B37, op_0B37);
    script_register(0x0B43, op_0B43);
    script_register(0x0B4F, op_0B4F);
    script_register(0x0B63, op_0B63);
    script_register(0x0C8F, op_0C8F);
    script_register(0x0C90, op_0C90);
    raknet_install();
    dialogs_install();
    textdraws_install();
    players_install();
    console_install();
    return 1;
}
