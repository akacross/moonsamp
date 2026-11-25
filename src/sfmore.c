/* More SAMPFUNCS opcodes for CLEO scripts, as SAMPFUNCS 5.7.1 does them:
 * screen coordinates, emulated keys, timers, downloads and global
 * functions. */
#include "moonsamp.h"

#define VIEW_MATRIX    ((const float *)0xB6FA2C)   /* TheCamera.m_mViewMatrix */
#define SCREEN_W       (*(const int *)0xC17044)     /* RsGlobal.maximumWidth */
#define SCREEN_H       (*(const int *)0xC17048)
#define ACTIVE_SCRIPTS (*(Script **)0xA8B42C)
#define SCRIPT_SIZE    0xE0                         /* sizeof(CRunningScript) */
#define MAX_TIMERS     64
#define MAX_DOWNLOADS  32
#define MAX_FUNCS      64
#define MAX_FRAMES     32

static int alive(const Script *t)
{
    Script *s;
    for (s = ACTIVE_SCRIPTS; s; s = s->next)
        if (s == t)
            return 1;
    return 0;
}

/* CLEO's ThreadJumpAtLabelPtr: a negative label is an offset into the
 * script's own code, a positive one into main.scm. */
static void jump(Script *s, int32_t label)
{
    s->ip = label < 0 ? s->base_ip - label : script_space() + label;
}

static int32_t to_int(float f) { return (int32_t)f; }

/* ---- screen coordinates ---------------------------------------------- */

/* 0B5F convert_window_screen_coords %1d% %2d% to_game_screen_coords %3d% %4d% */
static int8_t __stdcall op_0B5F(Script *s)
{
    int32_t x, y;
    script_collect(s, 2);
    x = ScriptParams[0].i, y = ScriptParams[1].i;
    ScriptParams[0].f = SCREEN_W ? (float)x * 640.0f / (float)SCREEN_W : 0.0f;
    ScriptParams[1].f = SCREEN_H ? (float)y * 448.0f / (float)SCREEN_H : 0.0f;
    script_store(s, 2);
    return 0;
}

/* 0B60 convert_game_screen_coords %1d% %2d% to_window_screen_coords %3d% %4d% */
static int8_t __stdcall op_0B60(Script *s)
{
    float x, y;
    script_collect(s, 2);
    x = ScriptParams[0].f, y = ScriptParams[1].f;
    ScriptParams[0].i = to_int(x * (float)SCREEN_W / 640.0f);
    ScriptParams[1].i = to_int(y * (float)SCREEN_H / 448.0f);
    script_store(s, 2);
    return 0;
}

/* The inverse of a 3x3 matrix given by rows; 0 if it has none. */
static int inverse3(const float m[9], float out[9])
{
    float det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6])
              + m[2] * (m[3] * m[7] - m[4] * m[6]);
    float r;
    if (det > -1e-12f && det < 1e-12f)
        return 0;
    r = 1.0f / det;
    out[0] = (m[4] * m[8] - m[5] * m[7]) * r;
    out[1] = (m[2] * m[7] - m[1] * m[8]) * r;
    out[2] = (m[1] * m[5] - m[2] * m[4]) * r;
    out[3] = (m[5] * m[6] - m[3] * m[8]) * r;
    out[4] = (m[0] * m[8] - m[2] * m[6]) * r;
    out[5] = (m[2] * m[3] - m[0] * m[5]) * r;
    out[6] = (m[3] * m[7] - m[4] * m[6]) * r;
    out[7] = (m[1] * m[6] - m[0] * m[7]) * r;
    out[8] = (m[0] * m[4] - m[1] * m[3]) * r;
    return 1;
}

/* 0B8F convert_screen_coords %1d% %2d% depth %3d% to_world_3d %4d% %5d% %6d%:
 * CalcScreenCoors backwards (it divides by depth and scales by the screen). */
static int8_t __stdcall op_0B8F(Script *s)
{
    const float *v = VIEW_MATRIX;
    float rows[9] = { v[0], v[1], v[2], v[4], v[5], v[6], v[8], v[9], v[10] }, inv[9], d[3];
    float x, y, z;
    int j;
    script_collect(s, 3);
    x = (float)(uint32_t)ScriptParams[0].i;
    y = (float)(uint32_t)ScriptParams[1].i;
    z = ScriptParams[2].f;
    if (!SCREEN_W || !SCREEN_H || !inverse3(rows, inv)) {
        ScriptParams[0].f = ScriptParams[1].f = ScriptParams[2].f = 0.0f;
    } else {
        /* view = world.x * right + world.y * forward + world.z * up + pos */
        d[0] = x * z / (float)SCREEN_W - v[12];
        d[1] = y * z / (float)SCREEN_H - v[13];
        d[2] = z - v[14];
        for (j = 0; j < 3; j++)
            ScriptParams[j].f = d[0] * inv[j] + d[1] * inv[3 + j] + d[2] * inv[6 + j];
    }
    script_store(s, 3);
    return 0;
}

/* ---- emulated keys ----------------------------------------------------- */

static void send_key(int vk, int down)
{
    INPUT in;
    UINT scan = MapVirtualKeyA((UINT)vk, 0);
    if (!scan)
        return;
    memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = (WORD)scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &in, sizeof in);
}

/* 0C72 set_virtual_key %1d% down %2d% */
static int8_t __stdcall op_0C72(Script *s)
{
    script_collect(s, 2);
    send_key(ScriptParams[0].i, ScriptParams[1].i != 0);
    return 0;
}

/* 0C73 set_char_key %1d% down %2d% */
static int8_t __stdcall op_0C73(Script *s)
{
    script_collect(s, 2);
    send_key(VkKeyScanA((CHAR)ScriptParams[0].i) & 0xFF, ScriptParams[1].i != 0);
    return 0;
}

/* ---- timers -------------------------------------------------------------- */

/* A timer runs its label in the script that made it, every interval, the
 * script's state put back afterwards; the label ends with 0B43. */
static struct { int id, active; Script *script; int32_t label; uint32_t interval, start; } timers[MAX_TIMERS];
static int next_timer;

static int timer_slot(int id)
{
    int i;
    for (i = 0; i < MAX_TIMERS; i++)
        if (id && timers[i].id == id)
            return i;
    return -1;
}

/* 0C74 %3d% = create_timer_interval %1d% on_label %2d% */
static int8_t __stdcall op_0C74(Script *s)
{
    int i;
    script_collect(s, 2);
    for (i = 0; i < MAX_TIMERS && timers[i].id; i++)
        ;
    if (i < MAX_TIMERS) {
        timers[i].id = ++next_timer;
        timers[i].active = 1;
        timers[i].script = s;
        timers[i].interval = (uint32_t)ScriptParams[0].i;
        timers[i].label = ScriptParams[1].i;
        timers[i].start = GetTickCount();
    }
    ScriptParams[0].i = i < MAX_TIMERS ? timers[i].id : 0;
    script_store(s, 1);
    script_cond(s, i < MAX_TIMERS);
    return 0;
}

/* One timer opcode: the slot of its first argument, cond whether there is one. */
static int timer_arg(Script *s, int extra)
{
    int i;
    script_collect(s, 1 + extra);
    i = timer_slot(ScriptParams[0].i);
    return i;
}

/* 0C75 kill_timer %1d% */
static int8_t __stdcall op_0C75(Script *s)
{
    int i = timer_arg(s, 0);
    if (i >= 0)
        timers[i].id = 0;
    script_cond(s, i >= 0);
    return 0;
}

/* 0C76 reset_timer %1d% */
static int8_t __stdcall op_0C76(Script *s)
{
    int i = timer_arg(s, 0);
    if (i >= 0)
        timers[i].start = GetTickCount();
    script_cond(s, i >= 0);
    return 0;
}

/* 0C77 set_timer %1d% interval %2d% */
static int8_t __stdcall op_0C77(Script *s)
{
    int i = timer_arg(s, 1);
    if (i >= 0)
        timers[i].interval = (uint32_t)ScriptParams[1].i;
    script_cond(s, i >= 0);
    return 0;
}

/* 0C78 set_timer %1d% active %2d% */
static int8_t __stdcall op_0C78(Script *s)
{
    int i = timer_arg(s, 1);
    if (i >= 0)
        timers[i].active = ScriptParams[1].i != 0;
    script_cond(s, i >= 0);
    return 0;
}

/* 0C79 is_timer %1d% active */
static int8_t __stdcall op_0C79(Script *s)
{
    int i = timer_arg(s, 0);
    script_cond(s, i >= 0 && timers[i].active);
    return 0;
}

static void timer_value(Script *s, int i, int32_t v)
{
    ScriptParams[0].i = i >= 0 ? v : 0;
    script_store(s, 1);
    script_cond(s, i >= 0);
}

/* 0C7A %2d% = get_timer %1d% interval */
static int8_t __stdcall op_0C7A(Script *s)
{
    int i = timer_arg(s, 0);
    timer_value(s, i, i >= 0 ? (int32_t)timers[i].interval : 0);
    return 0;
}

/* 0C7B %2d% = get_timer %1d% time_elapsed */
static int8_t __stdcall op_0C7B(Script *s)
{
    int i = timer_arg(s, 0);
    timer_value(s, i, i >= 0 ? (int32_t)(GetTickCount() - timers[i].start) : 0);
    return 0;
}

/* 0C7C %2d% = get_timer %1d% time_left */
static int8_t __stdcall op_0C7C(Script *s)
{
    int i = timer_arg(s, 0);
    int32_t left = 0;
    if (i >= 0) {
        uint32_t gone = GetTickCount() - timers[i].start;
        left = gone < timers[i].interval ? (int32_t)(timers[i].interval - gone) : 0;
    }
    timer_value(s, i, left);
    return 0;
}

static void timers_tick(void)
{
    uint8_t saved[SCRIPT_SIZE];
    uint32_t now = GetTickCount();
    int i;
    for (i = 0; i < MAX_TIMERS; i++) {
        Script *t = timers[i].script;
        if (!timers[i].id)
            continue;
        if (!alive(t)) {
            timers[i].id = 0;
            continue;
        }
        if (!timers[i].active || now - timers[i].start < timers[i].interval)
            continue;
        memcpy(saved, t, sizeof saved);
        script_run_label(t, timers[i].label);
        memcpy(t, saved, sizeof saved);
        timers[i].start = GetTickCount();
    }
}

/* ---- downloads ------------------------------------------------------------ */

/* A download's state is the last URLMON status (BINDSTATUS_*), as
 * SAMPFUNCS reports it: 6 (ENDDOWNLOADDATA) when done, -1 when it failed. */
typedef struct Download Download;
typedef struct {
    HRESULT (WINAPI *QueryInterface)(Download *, const IID *, void **);
    ULONG (WINAPI *AddRef)(Download *);
    ULONG (WINAPI *Release)(Download *);
    HRESULT (WINAPI *OnStartBinding)(Download *, DWORD, void *);
    HRESULT (WINAPI *GetPriority)(Download *, LONG *);
    HRESULT (WINAPI *OnLowResource)(Download *, DWORD);
    HRESULT (WINAPI *OnProgress)(Download *, ULONG, ULONG, ULONG, LPCWSTR);
    HRESULT (WINAPI *OnStopBinding)(Download *, HRESULT, LPCWSTR);
    HRESULT (WINAPI *GetBindInfo)(Download *, DWORD *, void *);
    HRESULT (WINAPI *OnDataAvailable)(Download *, DWORD, DWORD, void *, void *);
    HRESULT (WINAPI *OnObjectAvailable)(Download *, const IID *, void *);
} DownloadVtbl;
struct Download {
    const DownloadVtbl *vtbl;
    int id;
    volatile LONG state, done;
    char url[256], path[MAX_PATH];
};

static HRESULT WINAPI dl_query(Download *d, const IID *iid, void **out) { (void)iid; *out = d; return S_OK; }
static ULONG WINAPI dl_ref(Download *d) { (void)d; return 1; }
static HRESULT WINAPI dl_start(Download *d, DWORD r, void *b) { (void)d; (void)r; (void)b; return S_OK; }
static HRESULT WINAPI dl_priority(Download *d, LONG *p) { (void)d; *p = 0; return S_OK; }
static HRESULT WINAPI dl_low(Download *d, DWORD r) { (void)d; (void)r; return S_OK; }
static HRESULT WINAPI dl_progress(Download *d, ULONG p, ULONG m, ULONG code, LPCWSTR t)
{
    (void)p; (void)m; (void)t;
    InterlockedExchange(&d->state, (LONG)code);
    return S_OK;
}
static HRESULT WINAPI dl_stop(Download *d, HRESULT h, LPCWSTR t) { (void)d; (void)h; (void)t; return S_OK; }
static HRESULT WINAPI dl_info(Download *d, DWORD *f, void *i) { (void)d; (void)f; (void)i; return E_NOTIMPL; }
static HRESULT WINAPI dl_data(Download *d, DWORD f, DWORD n, void *a, void *b) { (void)d; (void)f; (void)n; (void)a; (void)b; return S_OK; }
static HRESULT WINAPI dl_object(Download *d, const IID *i, void *o) { (void)d; (void)i; (void)o; return S_OK; }

static const DownloadVtbl dl_vtbl = { dl_query, dl_ref, dl_ref, dl_start, dl_priority, dl_low,
                                      dl_progress, dl_stop, dl_info, dl_data, dl_object };

typedef HRESULT (WINAPI *URLDownloadToFileFn)(void *, LPCSTR, LPCSTR, DWORD, void *);
static Download *downloads[MAX_DOWNLOADS];
static int next_download;

static DWORD WINAPI download_thread(void *arg)
{
    Download *d = arg;
    HMODULE urlmon = LoadLibraryA("urlmon.dll");
    URLDownloadToFileFn fn = urlmon ? (URLDownloadToFileFn)(void *)GetProcAddress(urlmon, "URLDownloadToFileA") : NULL;
    HRESULT hr = fn ? fn(NULL, d->url, d->path, 0, d) : E_FAIL;
    InterlockedExchange(&d->state, SUCCEEDED(hr) ? 6 : -1);
    InterlockedExchange(&d->done, 1);
    return 0;
}

static int download_slot(int id)
{
    int i;
    for (i = 0; i < MAX_DOWNLOADS; i++)
        if (id && downloads[i] && downloads[i]->id == id)
            return i;
    return -1;
}

/* 0C65 %3d% = download_url %1d% to_file %2d% */
static int8_t __stdcall op_0C65(Script *s)
{
    char url[256], path[MAX_PATH];
    Download *d = NULL;
    HANDLE t;
    int i;
    script_string(s, url, sizeof url);
    script_string(s, path, sizeof path);
    for (i = 0; i < MAX_DOWNLOADS && downloads[i]; i++)
        ;
    if (i < MAX_DOWNLOADS && url[0] && path[0]) {
        d = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *d);
        if (d) {
            d->vtbl = &dl_vtbl;
            d->id = ++next_download;
            lstrcpynA(d->url, url, sizeof d->url);
            lstrcpynA(d->path, path, sizeof d->path);
            t = CreateThread(NULL, 0, download_thread, d, 0, NULL);
            if (t) {
                CloseHandle(t);
                downloads[i] = d;
            } else {
                HeapFree(GetProcessHeap(), 0, d);
                d = NULL;
            }
        }
    }
    ScriptParams[0].i = d ? d->id : 0;
    script_store(s, 1);
    return 0;
}

/* 0C66 %2d% = get_download %1d% state */
static int8_t __stdcall op_0C66(Script *s)
{
    int i = download_slot(script_int(s));
    ScriptParams[0].i = i >= 0 ? downloads[i]->state : 0;
    script_store(s, 1);
    script_cond(s, i >= 0);
    return 0;
}

/* 0C7D release_download %1d%: a running one is let go when it finishes */
static int8_t __stdcall op_0C7D(Script *s)
{
    int i = download_slot(script_int(s));
    if (i >= 0) {
        if (downloads[i]->done) {
            HeapFree(GetProcessHeap(), 0, downloads[i]);
            downloads[i] = NULL;
        } else {
            downloads[i]->id = -1;   /* freed by downloads_tick once it ends */
        }
    }
    script_cond(s, i >= 0);
    return 0;
}

static void downloads_tick(void)
{
    int i;
    for (i = 0; i < MAX_DOWNLOADS; i++)
        if (downloads[i] && downloads[i]->id == -1 && downloads[i]->done) {
            HeapFree(GetProcessHeap(), 0, downloads[i]);
            downloads[i] = NULL;
        }
}

/* ---- global functions ------------------------------------------------------- */

/* A global function is a label in the script that registered it. The
 * calling script runs it itself, like a gosub into that script: 0C35 saves
 * the caller (its base, IP after the arguments, active flag and locals),
 * puts the arguments in its locals and jumps to the label with the
 * function's base; 0C36 puts the caller back and writes the results into
 * the variables left after 0C35's arguments. */
static struct { char name[64]; Script *owner; int32_t label; } funcs[MAX_FUNCS];
static struct { Script *script; uint8_t *base, *ip; uint8_t active; int32_t locals[32]; } frames[MAX_FRAMES];
static int frame_count;

static int func_slot(const char *name)
{
    int i;
    for (i = 0; i < MAX_FUNCS; i++)
        if (funcs[i].owner && lstrcmpiA(funcs[i].name, name) == 0)
            return i;
    return -1;
}

/* 0C34 register_global_func %1d% to_label %2d% */
static int8_t __stdcall op_0C34(Script *s)
{
    char name[64];
    int32_t label;
    int i;
    script_string(s, name, sizeof name);
    label = script_int(s);
    i = func_slot(name);
    if (i >= 0 && alive(funcs[i].owner) && funcs[i].owner != s) {
        script_cond(s, 0);
        return 0;
    }
    if (i < 0)
        for (i = 0; i < MAX_FUNCS && funcs[i].owner && alive(funcs[i].owner); i++)
            ;
    if (i >= MAX_FUNCS || !name[0]) {
        script_cond(s, 0);
        return 0;
    }
    lstrcpynA(funcs[i].name, name, sizeof funcs[i].name);
    funcs[i].owner = s;
    funcs[i].label = label;
    script_cond(s, 1);
    return 0;
}

/* 0C35 call_global_func %1d% params %2d% args... results... */
static int8_t __stdcall op_0C35(Script *s)
{
    char name[64];
    int32_t args[32];
    int n, i, f;
    script_string(s, name, sizeof name);
    n = script_int(s);
    if (n < 0)
        n = 0;
    for (i = 0; i < n; i++) {
        script_collect(s, 1);
        if (i < 32)
            args[i] = ScriptParams[0].i;
    }
    f = func_slot(name);
    if (f < 0 || !alive(funcs[f].owner) || frame_count >= MAX_FRAMES) {
        script_skip_varargs(s);
        script_cond(s, 0);
        return 0;
    }
    if (n > 32)
        n = 32;
    script_cond(s, 1);
    frames[frame_count].script = s;
    frames[frame_count].base = s->base_ip;
    frames[frame_count].ip = s->ip;
    frames[frame_count].active = s->is_active;
    memcpy(frames[frame_count].locals, s->locals, sizeof frames[frame_count].locals);
    frame_count++;
    s->is_active = 1;
    memcpy(s->locals, args, (size_t)n * 4);
    s->base_ip = funcs[f].owner->base_ip;
    jump(s, funcs[f].label);
    return 0;
}

/* 0C36 func_ret %1d% values...: back to the caller of 0C35 */
static int8_t __stdcall op_0C36(Script *s)
{
    int32_t values[32];
    int n, i, k;
    n = script_int(s);
    if (n < 0)
        n = 0;
    for (i = 0; i < n; i++) {
        script_collect(s, 1);
        if (i < 32)
            values[i] = ScriptParams[0].i;
    }
    if (n > 32)
        n = 32;
    for (k = frame_count - 1; k >= 0 && frames[k].script != s; k--)
        ;
    if (k < 0) {
        script_skip_varargs(s);
        script_cond(s, 0);
        return 0;
    }
    s->base_ip = frames[k].base;
    s->ip = frames[k].ip;
    s->is_active = frames[k].active;
    memcpy(s->locals, frames[k].locals, sizeof frames[k].locals);
    for (i = k; i < frame_count - 1; i++)
        frames[i] = frames[i + 1];
    frame_count--;
    for (i = 0; i < n; i++)
        ScriptParams[i].i = values[i];
    if (n)
        script_store(s, n);
    script_skip_varargs(s);
    return 0;
}

/* 0C37 is_global_func %1d% defined */
static int8_t __stdcall op_0C37(Script *s)
{
    char name[64];
    int i;
    script_string(s, name, sizeof name);
    i = func_slot(name);
    script_cond(s, i >= 0 && alive(funcs[i].owner));
    return 0;
}

/* 0C38 get_global_func %1d% thread %2d% label %3d% */
static int8_t __stdcall op_0C38(Script *s)
{
    char name[64];
    int i;
    script_string(s, name, sizeof name);
    i = func_slot(name);
    if (i >= 0 && !alive(funcs[i].owner))
        i = -1;
    ScriptParams[0].p = i >= 0 ? funcs[i].owner : NULL;
    ScriptParams[1].i = i >= 0 ? funcs[i].label : 0;
    script_store(s, 2);
    script_cond(s, i >= 0);
    return 0;
}

/* 0C39 unregister_global_func %1d% */
static int8_t __stdcall op_0C39(Script *s)
{
    char name[64];
    int i;
    script_string(s, name, sizeof name);
    i = func_slot(name);
    if (i >= 0)
        funcs[i].owner = NULL;
    return 0;
}

/* Frames of scripts that ended go. */
static void frames_tick(void)
{
    int i, k = 0;
    for (i = 0; i < frame_count; i++)
        if (alive(frames[i].script))
            frames[k++] = frames[i];
    frame_count = k;
}

void sfmore_tick(void)
{
    timers_tick();
    downloads_tick();
    frames_tick();
}

void sfmore_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B5F, op_0B5F }, { 0x0B60, op_0B60 }, { 0x0B8F, op_0B8F },
        { 0x0C72, op_0C72 }, { 0x0C73, op_0C73 },
        { 0x0C74, op_0C74 }, { 0x0C75, op_0C75 }, { 0x0C76, op_0C76 }, { 0x0C77, op_0C77 },
        { 0x0C78, op_0C78 }, { 0x0C79, op_0C79 }, { 0x0C7A, op_0C7A }, { 0x0C7B, op_0C7B },
        { 0x0C7C, op_0C7C },
        { 0x0C65, op_0C65 }, { 0x0C66, op_0C66 }, { 0x0C7D, op_0C7D },
        { 0x0C34, op_0C34 }, { 0x0C35, op_0C35 }, { 0x0C36, op_0C36 }, { 0x0C37, op_0C37 },
        { 0x0C38, op_0C38 }, { 0x0C39, op_0C39 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
