/* CLEO 5's own opcodes (0DD5, 2000-2801) without CLEO, after CLEO 5's
 * source (MIT, Copyright (c) 2007-2023 CLEO Library by Seemann, Alien, Deji
 * and Miran): input, debug output, files, memory, audio streams, text and
 * FXT labels, math and INI files. With 0A9A-0A9C to open the files the
 * file opcodes take.
 *
 * moonloader/lib/cleo5.lua runs them through moonsamp_run. With CLEO
 * installed none of them is registered with it: CLEO 5 has its own, and its
 * scripts expect those; text labels then go to CLEO's. Audio streams are
 * BASS channels: the lib takes MoonLoader's AudioStream handles to theirs. */
#include "moonsamp.h"

#define TEXT_OBJ            ((void *)0xC1B340)          /* TheText */
#define TEXT_GET            ((uint8_t *)0x6A0050)       /* CText::Get */
#define INTRO_TEXTS         ((uint8_t *)0xA913E8)       /* tScriptText[96], 0x44 each */
#define INTRO_TEXT_COUNT    (*(uint16_t *)0xA44B68)
#define CONTROLS_ACTIONS    ((uint32_t *)(0xB70198 + 0xB70)) /* [59][4] { key, order } */
#define ACTIVE_SCRIPTS      (*(Script **)0xA8B42C)
#define RS_GLOBAL_PS        0xC17054
#define WAKE_TIME(s)        AT(s, 0xCC, uint32_t)

typedef const char *(THISCALL *TextGetFn)(void *, const char *);
typedef void (__cdecl *InsertNumberFn)(const char *, int, int, int, int, int, int, char *);
typedef void (__cdecl *InsertKeysFn)(char *);
typedef const char *(__cdecl *UserDirFn)(void);

#define InsertNumberInString    ((InsertNumberFn)0x69DE90)
#define InsertControlKeys       ((InsertKeysFn)0x69E160)
#define UserDirectory           ((UserDirFn)0x744FB0)

static void store_int(Script *s, int32_t v) { ScriptParams[0].i = v; script_store(s, 1); }
static void store_float(Script *s, float v) { ScriptParams[0].f = v; script_store(s, 1); }
static void store_ptr(Script *s, const void *p) { store_int(s, (int32_t)(uintptr_t)p); }

static int is_text(Script *s) { return *s->ip > 0x08; }

static char lower(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static char upper(char c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

static int text_cmp(const char *a, const char *b, int nocase, size_t n)
{
    for (; n; n--, a++, b++) {
        char x = nocase ? lower(*a) : *a, y = nocase ? lower(*b) : *b;
        if (x != y)
            return (uint8_t)x - (uint8_t)y;
        if (!x)
            break;
    }
    return 0;
}

static const char *text_find(const char *s, const char *sub, int nocase)
{
    size_t n = lstrlenA(sub);
    for (; *s; s++)
        if (!text_cmp(s, sub, nocase, n))
            return s;
    return n ? NULL : s;
}

/* ---- formatting ------------------------------------------------------------ */

static size_t put_text(char *out, size_t at, size_t size, const char *s, int width, int left)
{
    int n = lstrlenA(s), pad = width > n ? width - n : 0;
    while (!left && pad-- > 0 && at + 1 < size)
        out[at++] = ' ';
    while (*s && at + 1 < size)
        out[at++] = *s++;
    while (left && pad-- > 0 && at + 1 < size)
        out[at++] = ' ';
    return at;
}

static void format_uint(char *buf, uint32_t v, int base, int caps, int digits, char fill)
{
    char tmp[16];
    int n = 0, i = 0;
    do {
        int d = (int)(v % (uint32_t)base);
        tmp[n++] = (char)(d < 10 ? '0' + d : (caps ? 'A' : 'a') + d - 10);
        v /= (uint32_t)base;
    } while (v);
    while (n < digits && n < 15)
        tmp[n++] = fill;
    while (n)
        buf[i++] = tmp[--n];
    buf[i] = 0;
}

static void format_float(char *buf, float f, int prec)
{
    double v = f, frac;
    uint32_t whole;
    char *p = buf;
    int i;
    if (prec < 0)
        prec = 6;
    if (prec > 9)
        prec = 9;
    if (v != v) {
        lstrcpyA(buf, "nan");
        return;
    }
    if (v < 0) {
        *p++ = '-';
        v = -v;
    }
    for (i = 0, frac = 0.5; i < prec; i++)
        frac /= 10;
    v += frac;
    if (v > 4294967295.0) {
        lstrcpyA(p, "inf");
        return;
    }
    whole = (uint32_t)v;
    format_uint(p, whole, 10, 0, 0, '0');
    p += lstrlenA(p);
    if (prec) {
        *p++ = '.';
        v -= whole;
        for (i = 0; i < prec; i++) {
            v *= 10;
            *p++ = (char)('0' + (int)v % 10);
            v -= (int)v;
        }
    }
    *p = 0;
}

/* CLEO's formatted text: a printf format, then its arguments up to the end
 * of the argument list (d i u x X o c s f e g p %), and ~k~ keys. */
static void read_formatted(Script *s, char *out, size_t size)
{
    char fmt[512], buf[256], num[48];
    const char *f;
    size_t at = 0;
    script_string(s, fmt, sizeof fmt);
    for (f = fmt; *f && at + 1 < size; f++) {
        int left = 0, zero = 0, width = 0, prec = -1;
        if (*f != '%') {
            out[at++] = *f;
            continue;
        }
        f++;
        if (*f == '%') {
            out[at++] = '%';
            continue;
        }
        for (; *f == '-' || *f == '0' || *f == '+' || *f == ' ' || *f == '#'; f++)
            left |= *f == '-', zero |= *f == '0';
        for (; *f >= '0' && *f <= '9'; f++)
            width = width * 10 + (*f - '0');
        if (*f == '.')
            for (prec = 0, f++; *f >= '0' && *f <= '9'; f++)
                prec = prec * 10 + (*f - '0');
        while (*f == 'l' || *f == 'h')
            f++;
        if (!*f)
            break;
        if (!*s->ip) {
            at = put_text(out, at, size, "", width, left);
            continue;
        }
        switch (*f) {
        case 's':
            at = put_text(out, at, size, script_string(s, buf, sizeof buf), width, left);
            break;
        case 'c':
            num[0] = (char)script_int(s);
            num[1] = 0;
            at = put_text(out, at, size, num, width, left);
            break;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
            format_float(num, script_float(s), prec);
            at = put_text(out, at, size, num, width, left);
            break;
        case 'd': case 'i': {
            int32_t v = script_int(s);
            uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
            int digits = zero && !left && width > 0 ? width - (v < 0) : prec;
            num[0] = '-';
            format_uint(num + (v < 0), u, 10, 0, digits, '0');
            at = put_text(out, at, size, num, width, left);
            break;
        }
        default: {
            int base = *f == 'o' ? 8 : (*f == 'x' || *f == 'X' || *f == 'p') ? 16 : 10;
            format_uint(num, (uint32_t)script_int(s), base, *f == 'X' || *f == 'p',
                        zero && !left ? width : (*f == 'p' ? 8 : prec), '0');
            at = put_text(out, at, size, num, width, left);
        }
        }
    }
    out[at] = 0;
    script_skip_varargs(s);
    InsertControlKeys(out);
}

/* ---- paths ------------------------------------------------------------------ */

static int prefix(const char *path, const char *pre, const char **rest)
{
    size_t n = lstrlenA(pre);
    if (text_cmp(path, pre, 1, n) || (path[n] && path[n] != '\\' && path[n] != '/'))
        return 0;
    *rest = path[n] ? path + n + 1 : path + n;
    return 1;
}

/* CLEO's virtual paths: root:, user:, cleo:, modules: and "." (here the
 * moonloader folder); anything else relative is the game folder's. */
static void resolve(const char *path, char *out, size_t size)
{
    const char *rest = path, *base = ms_game_dir();
    char dir[MAX_PATH];
    int i;
    if (prefix(path, "root:", &rest)) {
        lstrcpynA(dir, base, sizeof dir);
    } else if (prefix(path, "user:", &rest)) {
        lstrcpynA(dir, UserDirectory(), sizeof dir - 1);
        lstrcatA(dir, "\\");
    } else if (prefix(path, "cleo:", &rest)) {
        wsprintfA(dir, "%scleo\\", base);
    } else if (prefix(path, "modules:", &rest)) {
        wsprintfA(dir, "%scleo\\cleo_modules\\", base);
    } else if (prefix(path, ".", &rest)) {
        wsprintfA(dir, "%smoonloader\\", base);
    } else if ((path[0] && path[1] == ':') || path[0] == '\\' || path[0] == '/') {
        dir[0] = 0;
    } else {
        lstrcpynA(dir, base, sizeof dir);
    }
    if (lstrlenA(dir) + lstrlenA(rest) + 1 > (int)size) {
        out[0] = 0;
        return;
    }
    wsprintfA(out, "%s%s", dir, rest);
    for (i = 0; out[i]; i++)
        if (out[i] == '/')
            out[i] = '\\';
}

static void read_path(Script *s, char *out, size_t size)
{
    char buf[MAX_PATH];
    resolve(script_string(s, buf, sizeof buf), out, size);
}

/* ---- FXT texts, served by a hook on CText::Get ------------------------------ */

#define FXT_SLOTS 4096
static struct { char key[16]; char *text; } fxt[FXT_SLOTS];
static int fxt_count;
static uint8_t *text_tramp;

static unsigned fxt_hash(const char *key)
{
    unsigned h = 2166136261u;
    for (; *key; key++)
        h = (h ^ (uint8_t)upper(*key)) * 16777619u;
    return h % FXT_SLOTS;
}

static int fxt_slot(const char *key, int add)
{
    unsigned h = fxt_hash(key), k;
    for (k = 0; k < FXT_SLOTS; k++, h = (h + 1) % FXT_SLOTS) {
        if (!fxt[h].key[0])
            return add ? (int)h : -1;
        if (!text_cmp(fxt[h].key, key, 1, sizeof fxt[h].key))
            return (int)h;
    }
    return -1;
}

static const char *fxt_get(const char *key)
{
    int i = fxt_count ? fxt_slot(key, 0) : -1;
    return i >= 0 && fxt[i].text ? fxt[i].text : NULL;
}

static const char *__attribute__((fastcall)) hook_text_get(void *text, int edx, const char *key)
{
    const char *found;
    (void)edx;
    if (!key[0] || key[0] == ' ')
        return "";
    found = fxt_get(key);
    return found ? found : ((TextGetFn)(void *)text_tramp)(text, key);
}

/* Installed with the first text: over SA-MP's own jump if it is there. */
static int hook_text(void)
{
    static const uint8_t prologue[5] = { 0x83, 0xEC, 0x20, 0x56, 0x57 };   /* sub esp,20h; push esi; push edi */
    uint8_t jmp[5] = { 0xE9 }, *t;
    if (text_tramp)
        return 1;
    t = VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t)
        return 0;
    if (TEXT_GET[0] == 0xE9) {
        uint8_t *to = TEXT_GET + 5 + *(int32_t *)(TEXT_GET + 1);
        t[0] = 0xE9;
        *(int32_t *)(t + 1) = (int32_t)(to - (t + 5));
    } else if (!memcmp(TEXT_GET, prologue, 5)) {
        memcpy(t, prologue, 5);
        t[5] = 0xE9;
        *(int32_t *)(t + 6) = (int32_t)((TEXT_GET + 5) - (t + 10));
    } else {
        ms_log("cleo5: CText::Get starts with %02X %02X; FXT texts are off", TEXT_GET[0], TEXT_GET[1]);
        VirtualFree(t, 0, MEM_RELEASE);
        return 0;
    }
    text_tramp = t;
    *(int32_t *)(jmp + 1) = (int32_t)((uint8_t *)(void *)hook_text_get - (TEXT_GET + 5));
    if (!ms_patch(TEXT_GET, jmp, 5)) {
        text_tramp = NULL;
        return 0;
    }
    ms_log("cleo5: CText::Get hooked for FXT texts");
    return 1;
}

/* With CLEO its own labels serve them (0ADF add, 0AE0 remove): CLEO 5
 * hooks CText::Get by putting the original bytes back for every call it does
 * not answer, which would take away a hook over its own. */
typedef int8_t (THISCALL *HandlerFn)(Script *, int);

static int cleo_label(const char *key, const char *text)
{
    uint8_t thread[0x100] __attribute__((aligned(4))), code[300], *p = code;
    int nk = lstrlenA(key), nt = text ? lstrlenA(text) : 0;
    int op = text ? 0x0ADF : 0x0AE0;
    if (nk > 7)
        return 0;
    if (nt > 255)
        nt = 255;
    *p++ = 0x0E;
    *p++ = (uint8_t)nk;
    memcpy(p, key, nk);
    p += nk;
    if (text) {
        *p++ = 0x0E;
        *p++ = (uint8_t)nt;
        memcpy(p, text, nt);
        p += nt;
    }
    memset(thread, 0, sizeof thread);
    memcpy(((Script *)thread)->name, "moonsamp", 8);
    ((Script *)thread)->base_ip = ((Script *)thread)->ip = code;
    (*(HandlerFn **)0x469FEE)[op / 100]((Script *)thread, op);
    return 1;
}

static int fxt_add(const char *key, const char *text)
{
    int i, n = lstrlenA(text);
    char *copy;
    if (script_cleo_present())
        return key[0] && cleo_label(key, text);
    if (!key[0] || lstrlenA(key) >= (int)sizeof fxt[0].key || !hook_text() || (i = fxt_slot(key, 1)) < 0)
        return 0;
    copy = HeapAlloc(GetProcessHeap(), 0, n + 1);
    if (!copy)
        return 0;
    memcpy(copy, text, n + 1);
    if (fxt[i].text)
        HeapFree(GetProcessHeap(), 0, fxt[i].text);
    else
        fxt_count++;
    lstrcpynA(fxt[i].key, key, sizeof fxt[i].key);
    fxt[i].text = copy;
    return 1;
}

/* A removed key keeps its slot (so the probe chain holds), without text. */
static int fxt_remove(const char *key)
{
    int i = fxt_count ? fxt_slot(key, 0) : -1;
    if (script_cleo_present())
        return key[0] && cleo_label(key, NULL);
    if (i < 0 || !fxt[i].text)
        return 0;
    HeapFree(GetProcessHeap(), 0, fxt[i].text);
    fxt[i].text = NULL;
    return 1;
}

/* An FXT file: "KEY text" lines, # and // comments. */
static int fxt_file(const char *path, int remove)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD size, got = 0;
    char *data, *line, *next;
    int count = 0;
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    size = GetFileSize(f, NULL);
    data = size < 0x1000000 ? HeapAlloc(GetProcessHeap(), 0, size + 1) : NULL;
    if (data && !ReadFile(f, data, size, &got, NULL))
        got = 0;
    CloseHandle(f);
    if (!data)
        return 0;
    data[got] = 0;
    for (line = data; line && *line; line = next) {
        char *k, *v, *end;
        next = line;
        while (*next && *next != '\n')
            next++;
        if (*next)
            *next++ = 0;
        else
            next = NULL;
        for (end = line; *end; end++)
            if (*end == '#' || (*end == '/' && end[1] == '/') || *end == '\r')
                break;
        *end = 0;
        for (k = line; *k && *k != ' ' && *k != '\t'; k++)
            ;
        if (!*k || k == line)
            continue;
        *k = 0;
        v = k + 1;
        while (end > v && (end[-1] == ' ' || end[-1] == '\t'))
            *--end = 0;
        count += remove ? fxt_remove(line) : fxt_add(line, v);
    }
    HeapFree(GetProcessHeap(), 0, data);
    return count;
}

/* ---- input ------------------------------------------------------------------ */

static uint8_t keys_now[256], keys_then[256];

void cleo5_tick(void)
{
    HWND *ps = *(HWND **)RS_GLOBAL_PS;
    memcpy(keys_then, keys_now, sizeof keys_now);
    if (ps && GetForegroundWindow() == *ps)
        GetKeyboardState(keys_now);
    else
        memset(keys_now, 0, sizeof keys_now);
}

static int key_down(int k) { return (keys_now[k] & 0x80) != 0; }
static int key_hit(int k) { return key_down(k) && !(keys_then[k] & 0x80); }

/* 2080 is_key_just_pressed %1d% */
static int8_t __stdcall op_2080(Script *s)
{
    int k = script_int(s);
    script_cond(s, (unsigned)k < 256 && key_hit(k));
    return 0;
}

/* 2081 / 2082 %3d% = get_key_pressed_in_range / get_key_just_pressed_in_range
 * %1d% %2d%: the variable is left alone when none is */
static int8_t key_in_range(Script *s, int just)
{
    int lo, hi, k;
    int32_t *var;
    script_collect(s, 2);
    lo = ScriptParams[0].i;
    hi = ScriptParams[1].i;
    var = script_var(s);
    for (k = lo < 0 ? 0 : lo; k <= hi && k < 256; k++)
        if (just ? key_hit(k) : key_down(k)) {
            if (var)
                *var = k;
            script_cond(s, 1);
            return 0;
        }
    script_cond(s, 0);
    return 0;
}
static int8_t __stdcall op_2081(Script *s) { return key_in_range(s, 0); }
static int8_t __stdcall op_2082(Script *s) { return key_in_range(s, 1); }

static void send_key(int vk, int down)
{
    INPUT in;
    memset(&in, 0, sizeof in);
    if (vk >= VK_LBUTTON && vk <= VK_XBUTTON2 && vk != VK_CANCEL) {
        in.type = INPUT_MOUSE;
        switch (vk) {
        case VK_LBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
        case VK_RBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
        case VK_MBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
        default:
            in.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            in.mi.mouseData = vk == VK_XBUTTON1 ? XBUTTON1 : XBUTTON2;
        }
    } else {
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = (WORD)vk;
        in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        if (vk != VK_RETURN && vk != VK_LSHIFT && vk != VK_LCONTROL && vk != VK_LMENU) {
            in.ki.wScan = (WORD)MapVirtualKeyA(vk, 4);     /* MAPVK_VK_TO_VSC_EX */
            in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
    }
    SendInput(1, &in, sizeof in);
}

/* 2083 emulate_key_press %1d%, 2084 emulate_key_release %1d% */
static int8_t __stdcall op_2083(Script *s) { int k = script_int(s); if ((unsigned)k < 256) send_key(k, 1); return 0; }
static int8_t __stdcall op_2084(Script *s) { int k = script_int(s); if ((unsigned)k < 256) send_key(k, 0); return 0; }

/* RsKeyCodes (1000-1055) and the controller's mouse buttons (1-7) as
 * virtual keys; anything else is its own code. */
static int rs_to_vk(uint32_t k)
{
    static const uint8_t from_rs[56] = {
        VK_ESCAPE, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
        VK_INSERT, VK_DELETE, VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
        VK_DIVIDE, VK_MULTIPLY, VK_ADD, VK_SUBTRACT, VK_DECIMAL, VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3,
        VK_NUMPAD4, VK_NUMPAD5, VK_NUMLOCK, VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9, VK_NUMPAD0,
        VK_RETURN, VK_SCROLL, VK_PAUSE, VK_BACK, VK_TAB, VK_CAPITAL, VK_RETURN, VK_LSHIFT, VK_RSHIFT,
        VK_SHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN, VK_APPS,
    };
    static const uint8_t mouse[8] = { 0, VK_LBUTTON, VK_MBUTTON, VK_RBUTTON, 0, 0, VK_XBUTTON1, VK_XBUTTON2 };
    if (k >= 1000 && k < 1056)
        return from_rs[k - 1000];
    if (k >= 1 && k <= 7)
        return mouse[k];
    return (int)k;
}

/* 2085 %3d% = get_controller_key %1d% alt_key %2d%: the action's keys in
 * their set order, without the mouse wheel */
static int8_t __stdcall op_2085(Script *s)
{
    int action, alt, i, j, n = 0;
    uint32_t key[4], order[4];
    int32_t *var;
    script_collect(s, 2);
    action = ScriptParams[0].i;
    alt = ScriptParams[1].i;
    var = script_var(s);
    if ((unsigned)action < 59 && (unsigned)alt < 4) {
        const uint32_t *a = CONTROLS_ACTIONS + action * 8;
        for (i = 0; i < 4; i++) {
            uint32_t k = a[i * 2], o = a[i * 2 + 1];
            if (!k || !o || k == 4 || k == 5)
                continue;
            for (j = n++; j > 0 && order[j - 1] > o; j--)
                key[j] = key[j - 1], order[j] = order[j - 1];
            key[j] = k;
            order[j] = o;
        }
    }
    if ((unsigned)action < 59 && alt >= 0 && alt < n) {
        if (var)
            *var = rs_to_vk(key[alt]);
        script_cond(s, 1);
    } else {
        script_cond(s, 0);
    }
    return 0;
}

static const char *gxt(const char *key) { return ((TextGetFn)(void *)TEXT_GET)(TEXT_OBJ, key); }

/* 2086 %2d% = get_key_name %1d% */
static int8_t __stdcall op_2086(Script *s)
{
    static char buf[64];
    static const struct { uint8_t vk; const char *gxt, *text; } names[] = {
        { VK_LBUTTON, "FEC_MSL", 0 }, { VK_RBUTTON, "FEC_MSR", 0 }, { VK_MBUTTON, "FEC_MSM", 0 },
        { VK_XBUTTON1, "FEC_MXO", 0 }, { VK_XBUTTON2, "FEC_MXT", 0 }, { VK_BACK, "FEC_BSP", 0 },
        { VK_TAB, "FEC_TAB", 0 }, { VK_CLEAR, 0, "CLEAR" }, { VK_RETURN, "FEC_RTN", 0 },
        { VK_SHIFT, "FEC_SFT", 0 }, { VK_CONTROL, 0, "CTRL" }, { VK_MENU, 0, "ALT" },
        { VK_PAUSE, "FEC_PSB", 0 }, { VK_CAPITAL, "FEC_CLK", 0 }, { VK_ESCAPE, 0, "ESC" },
        { VK_SPACE, "FEC_SPC", 0 }, { VK_PRIOR, "FEC_PGU", 0 }, { VK_NEXT, "FEC_PGD", 0 },
        { VK_END, "FEC_END", 0 }, { VK_HOME, "FEC_HME", 0 }, { VK_LEFT, "FEC_LFA", 0 },
        { VK_UP, "FEC_UPA", 0 }, { VK_RIGHT, "FEC_RFA", 0 }, { VK_DOWN, "FEC_DWA", 0 },
        { VK_SELECT, 0, "SELECT" }, { VK_PRINT, 0, "PRINT" }, { VK_EXECUTE, 0, "EXECUTE" },
        { VK_SNAPSHOT, 0, "PRTSCR" }, { VK_INSERT, "FEC_IRT", 0 }, { VK_DELETE, "FEC_DLL", 0 },
        { VK_HELP, 0, "HELP" }, { VK_LWIN, "FEC_LWD", 0 }, { VK_RWIN, "FEC_RWD", 0 },
        { VK_APPS, "FEC_WRC", 0 }, { VK_SLEEP, 0, "SLEEP" }, { VK_MULTIPLY, "FECSTAR", 0 },
        { VK_ADD, "FEC_PLS", 0 }, { VK_SUBTRACT, "FEC_MIN", 0 }, { VK_DECIMAL, "FEC_DOT", 0 },
        { VK_DIVIDE, "FEC_FWS", 0 }, { VK_NUMLOCK, "FEC_NLK", 0 }, { VK_SCROLL, "FEC_SLK", 0 },
        { VK_OEM_NEC_EQUAL, "FEC_ETR", 0 }, { VK_LSHIFT, "FEC_LSF", 0 }, { VK_RSHIFT, "FEC_RSF", 0 },
        { VK_LCONTROL, "FEC_LCT", 0 }, { VK_RCONTROL, "FEC_RCT", 0 }, { VK_LMENU, "FEC_LAL", 0 },
        { VK_RMENU, "FEC_RAL", 0 }, { VK_OEM_1, 0, ";" }, { VK_OEM_PLUS, 0, "=" },
        { VK_OEM_COMMA, 0, "," }, { VK_OEM_MINUS, 0, "-" }, { VK_OEM_PERIOD, 0, "." },
        { VK_OEM_2, 0, "/" }, { VK_OEM_3, 0, "`" }, { VK_OEM_4, 0, "[" }, { VK_OEM_5, 0, "\\" },
        { VK_OEM_6, 0, "]" }, { VK_OEM_7, 0, "'" },
    };
    int k = script_int(s);
    const char *name = NULL;
    size_t i;
    if ((k >= '0' && k <= '9') || (k >= 'A' && k <= 'Z')) {
        buf[0] = (char)k;
        buf[1] = 0;
        name = buf;
    } else if (k >= VK_F1 && k <= VK_F24) {
        InsertNumberInString(gxt("FEC_FNC"), k - VK_F1 + 1, 0, 0, 0, 0, 0, buf);
        name = buf;
    } else if (k >= VK_NUMPAD0 && k <= VK_NUMPAD9) {
        InsertNumberInString(gxt("FEC_NMN"), k - VK_NUMPAD0, 0, 0, 0, 0, 0, buf);
        name = buf;
    } else if (k == -1) {
        name = gxt("FEC_UNB");
    } else {
        for (i = 0; i < sizeof names / sizeof names[0]; i++)
            if (names[i].vk == k)
                name = names[i].text ? names[i].text : gxt(names[i].gxt);
    }
    if (name && name != buf) {
        lstrcpynA(buf, name, sizeof buf);
        name = buf;
    }
    store_ptr(s, name);
    script_cond(s, name != NULL);
    return 0;
}

/* ---- debug ------------------------------------------------------------------ */

static void debug_line(const char *what, const char *text)
{
    char line[600];
    wsprintfA(line, "%s%s", what, text);
    console_print(line);
    ms_log("%s", line);
}

/* 2100 breakpoint %1d% %2s% ...: logged; the game is not paused, nothing
 * here would resume it */
static int8_t __stdcall op_2100(Script *s)
{
    char text[512] = "";
    if (*s->ip == 0x04 || *s->ip == 0x01)
        script_int(s);
    if (*s->ip)
        read_formatted(s, text, sizeof text);
    else
        s->ip++;
    debug_line("breakpoint ", text);
    return 0;
}

/* 2101 trace %1s% ... */
static int8_t __stdcall op_2101(Script *s)
{
    char text[512];
    read_formatted(s, text, sizeof text);
    debug_line("trace: ", text);
    return 0;
}

/* 2102 log_to_file %1s% timestamp %2d% text %3s% ... */
static int8_t __stdcall op_2102(Script *s)
{
    char path[MAX_PATH], line[600], text[512];
    HANDLE f;
    DWORD w;
    int stamp;
    read_path(s, path, sizeof path);
    stamp = script_int(s);
    read_formatted(s, text, sizeof text);
    line[0] = 0;
    if (stamp) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        wsprintfA(line, "%02d/%02d/%04d %02d:%02d:%02d.%03d ", t.wDay, t.wMonth, t.wYear, t.wHour,
                  t.wMinute, t.wSecond, t.wMilliseconds);
    }
    lstrcatA(line, text);
    lstrcatA(line, "\r\n");
    f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        ms_log("cleo5: cannot open log file %s", path);
        return 0;
    }
    WriteFile(f, line, lstrlenA(line), &w, NULL);
    CloseHandle(f);
    return 0;
}

/* ---- files ------------------------------------------------------------------ */

#define MAX_FILES 64
static struct { HANDLE h; int append; } files[MAX_FILES];

static int file_slot(int32_t h)
{
    int i;
    for (i = 0; i < MAX_FILES; i++)
        if (files[i].h && (int32_t)(uintptr_t)files[i].h == h)
            return i;
    return -1;
}

/* 0A9A %3d% = openfile %1s% mode %2d%: a mode as fopen's (r w a, +, b t) */
static int8_t __stdcall op_0A9A(Script *s)
{
    char path[MAX_PATH], mode[16];
    DWORD access, create;
    HANDLE h;
    int i, plus;
    read_path(s, path, sizeof path);
    if (is_text(s)) {
        script_string(s, mode, sizeof mode);
    } else {
        int32_t m = script_int(s);
        memcpy(mode, &m, 4);
        mode[4] = 0;
    }
    plus = mode[0] && mode[1] && (mode[1] == '+' || mode[2] == '+');
    switch (lower(mode[0])) {
    case 'w': access = GENERIC_WRITE | (plus ? GENERIC_READ : 0); create = CREATE_ALWAYS; break;
    case 'a': access = GENERIC_WRITE | (plus ? GENERIC_READ : 0); create = OPEN_ALWAYS; break;
    default:  access = GENERIC_READ | (plus ? GENERIC_WRITE : 0); create = OPEN_EXISTING; break;
    }
    for (i = 0; i < MAX_FILES && files[i].h; i++)
        ;
    h = i < MAX_FILES ? CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, create, 0, NULL)
                      : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) {
        store_int(s, 0);
        script_cond(s, 0);
        return 0;
    }
    files[i].h = h;
    files[i].append = lower(mode[0]) == 'a';
    store_ptr(s, h);
    script_cond(s, 1);
    return 0;
}

/* 0A9B closefile %1d% */
static int8_t __stdcall op_0A9B(Script *s)
{
    int i = file_slot(script_int(s));
    if (i >= 0) {
        CloseHandle(files[i].h);
        files[i].h = NULL;
    }
    return 0;
}

static HANDLE file_arg(Script *s, int *slot)
{
    int i = file_slot(script_int(s));
    if (slot)
        *slot = i;
    return i >= 0 ? files[i].h : NULL;
}

/* 0A9C %2d% = file %1d% size */
static int8_t __stdcall op_0A9C(Script *s)
{
    HANDLE h = file_arg(s, NULL);
    store_int(s, h ? (int32_t)GetFileSize(h, NULL) : 0);
    return 0;
}

/* 2300 %2d% = get_file_position %1d% */
static int8_t __stdcall op_2300(Script *s)
{
    HANDLE h = file_arg(s, NULL);
    store_int(s, h ? (int32_t)SetFilePointer(h, 0, NULL, FILE_CURRENT) : -1);
    return 0;
}

/* 2301 read_block_from_file %1d% size %2d% buffer %3d%, 2302
 * write_block_to_file %1d% size %2d% address %3d% */
static int8_t block(Script *s, int write)
{
    int slot;
    HANDLE h = file_arg(s, &slot);
    DWORD done = 0;
    int32_t size;
    void *buf;
    script_collect(s, 2);
    size = ScriptParams[0].i;
    buf = ScriptParams[1].p;
    if (!h || size < 0 || (size && !buf)) {
        script_cond(s, 0);
        return 0;
    }
    if (!size) {
        script_cond(s, 1);
        return 0;
    }
    if (write && files[slot].append)
        SetFilePointer(h, 0, NULL, FILE_END);
    if (!(write ? WriteFile(h, buf, size, &done, NULL) : ReadFile(h, buf, size, &done, NULL)))
        done = 0;
    script_cond(s, done == (DWORD)size);
    return 0;
}
static int8_t __stdcall op_2301(Script *s) { return block(s, 0); }
static int8_t __stdcall op_2302(Script *s) { return block(s, 1); }

/* 2303 %2s% = resolve_filepath %1s% */
static int8_t __stdcall op_2303(Script *s)
{
    static char path[MAX_PATH];
    read_path(s, path, sizeof path);
    store_ptr(s, path);
    return 0;
}

/* 2304 %3s% = get_script_filename %1d% full_path %2d%: only CLEO's scripts
 * have one */
static int8_t __stdcall op_2304(Script *s)
{
    script_collect(s, 2);
    script_var(s);
    script_cond(s, 0);
    return 0;
}

/* 2305 get_file_write_time %1s% store_to %2d%..%8d% */
static int8_t __stdcall op_2305(Script *s)
{
    char path[MAX_PATH];
    HANDLE f;
    FILETIME ft;
    SYSTEMTIME utc, t;
    int ok;
    read_path(s, path, sizeof path);
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    ok = f != INVALID_HANDLE_VALUE && GetFileTime(f, NULL, NULL, &ft) && FileTimeToSystemTime(&ft, &utc) &&
         SystemTimeToTzSpecificLocalTime(NULL, &utc, &t);
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    if (!ok)
        memset(&t, 0, sizeof t);
    ScriptParams[0].i = t.wYear;
    ScriptParams[1].i = t.wMonth;
    ScriptParams[2].i = t.wDay;
    ScriptParams[3].i = t.wHour;
    ScriptParams[4].i = t.wMinute;
    ScriptParams[5].i = t.wSecond;
    ScriptParams[6].i = t.wMilliseconds;
    script_store(s, 7);
    script_cond(s, ok);
    return 0;
}

/* ---- memory and scripts ------------------------------------------------------ */

/* 2400 copy_memory %1d% to %2d% size %3d% */
static int8_t __stdcall op_2400(Script *s)
{
    uint8_t *src, *dst;
    int32_t n;
    script_collect(s, 3);
    src = ScriptParams[0].p;
    dst = ScriptParams[1].p;
    n = ScriptParams[2].i;
    if (n > 0 && src && dst) {
        if (dst < src || dst >= src + n) {
            memcpy(dst, src, (size_t)n);
        } else {
            while (n--)
                dst[n] = src[n];
        }
    }
    return 0;
}

/* 2401 read_memory_with_offset %1d% offset %2d% size %3d% store_to %4d% */
static int8_t __stdcall op_2401(Script *s)
{
    uint8_t *p;
    int32_t size, v = 0;
    script_collect(s, 3);
    p = ScriptParams[0].p ? (uint8_t *)ScriptParams[0].p + ScriptParams[1].i : NULL;
    size = ScriptParams[2].i;
    if (p && size > 0)
        memcpy(&v, p, size > 4 ? 4 : (size_t)size);
    store_int(s, v);
    return 0;
}

/* 2402 write_memory_with_offset %1d% offset %2d% size %3d% value %4d%: over
 * 4 bytes it fills with the value's low byte */
static int8_t __stdcall op_2402(Script *s)
{
    uint8_t *p;
    int32_t size, v;
    script_collect(s, 4);
    p = ScriptParams[0].p ? (uint8_t *)ScriptParams[0].p + ScriptParams[1].i : NULL;
    size = ScriptParams[2].i;
    v = ScriptParams[3].i;
    if (!p || size <= 0)
        return 0;
    if (size <= 4)
        memcpy(p, &v, (size_t)size);
    else
        memset(p, v & 0xFF, (size_t)size);
    return 0;
}

/* 2403 forget_memory %1d%: CLEO's allocation bookkeeping; none here */
static int8_t __stdcall op_2403(Script *s) { script_int(s); return 0; }

/* 2404 get_script_struct_just_created %1d%: the head of the active list */
static int8_t __stdcall op_2404(Script *s) { store_ptr(s, ACTIVE_SCRIPTS); return 0; }

static int running(Script *t)
{
    Script *a;
    for (a = ACTIVE_SCRIPTS; a; a = a->next)
        if (a == t)
            return 1;
    return 0;
}

/* 2405 is_script_running %1d% */
static int8_t __stdcall op_2405(Script *s) { script_cond(s, running((Script *)(uintptr_t)script_int(s))); return 0; }

/* 2406 %2d% = get_script_struct_from_filename %1s%: only CLEO's scripts have
 * files */
static int8_t __stdcall op_2406(Script *s)
{
    char buf[MAX_PATH];
    script_string(s, buf, sizeof buf);
    store_int(s, 0);
    script_cond(s, 0);
    return 0;
}

/* 2407 is_memory_equal %1d% %2d% size %3d% */
static int8_t __stdcall op_2407(Script *s)
{
    script_collect(s, 3);
    script_cond(s, ScriptParams[2].i <= 0 ||
                       (ScriptParams[0].p && ScriptParams[1].p &&
                        !memcmp(ScriptParams[0].p, ScriptParams[1].p, (size_t)ScriptParams[2].i)));
    return 0;
}

/* 2408 terminate_script %1d%: it never wakes again */
static int8_t __stdcall op_2408(Script *s)
{
    Script *t = (Script *)(uintptr_t)script_int(s);
    if (t && running(t))
        WAKE_TIME(t) = 0xFFFFFFFF;
    return 0;
}

/* ---- audio streams: BASS channels ------------------------------------------- */

typedef struct { DWORD freq, chans, flags, ctype, origres, plugin, sample; const char *filename; } ChannelInfo;
static struct {
    DWORD (WINAPI *active)(DWORD);
    uint64_t (WINAPI *length)(DWORD, DWORD);
    uint64_t (WINAPI *position)(DWORD, DWORD);
    BOOL (WINAPI *set_position)(DWORD, uint64_t, DWORD);
    double (WINAPI *to_seconds)(DWORD, uint64_t);
    BOOL (WINAPI *get_attr)(DWORD, DWORD, float *);
    BOOL (WINAPI *set_attr)(DWORD, DWORD, float);
    BOOL (WINAPI *slide)(DWORD, DWORD, float, DWORD);
    BOOL (WINAPI *info)(DWORD, ChannelInfo *);
    BOOL (WINAPI *set3d)(DWORD, int, float, float, int, int, float);
    void (WINAPI *apply3d)(void);
} bass;
#define BASS_ATTRIB_FREQ 1
#define BASS_ATTRIB_VOL  2

#define MAX_STREAMS 256
static struct { DWORD h; float freq, speed; int type; } streams[MAX_STREAMS];

static int bass_ready(void)
{
    static int tried;
    HMODULE m;
    if (bass.active || tried)
        return bass.active != NULL;
    tried = 1;
    m = GetModuleHandleA("bass.dll");
    if (!m)
        return 0;
#define P(field, name) *(FARPROC *)&bass.field = GetProcAddress(m, name)
    P(length, "BASS_ChannelGetLength");
    P(position, "BASS_ChannelGetPosition");
    P(set_position, "BASS_ChannelSetPosition");
    P(to_seconds, "BASS_ChannelBytes2Seconds");
    P(get_attr, "BASS_ChannelGetAttribute");
    P(set_attr, "BASS_ChannelSetAttribute");
    P(slide, "BASS_ChannelSlideAttribute");
    P(info, "BASS_ChannelGetInfo");
    P(set3d, "BASS_ChannelSet3DAttributes");
    P(apply3d, "BASS_Apply3D");
    P(active, "BASS_ChannelIsActive");
#undef P
    if (!bass.length || !bass.position || !bass.set_position || !bass.to_seconds || !bass.get_attr ||
        !bass.set_attr || !bass.slide || !bass.info || !bass.set3d || !bass.apply3d)
        bass.active = NULL;
    return bass.active != NULL;
}

/* The channel's entry: its own frequency (speed 1), speed and type. */
static int stream(DWORD h)
{
    ChannelInfo ci;
    int i, free_slot = -1;
    if (!h || !bass_ready() || !bass.info(h, &ci))
        return -1;
    for (i = 0; i < MAX_STREAMS; i++) {
        if (streams[i].h == h)
            return i;
        if (free_slot < 0 && (!streams[i].h || !bass.info(streams[i].h, &(ChannelInfo){ 0 })))
            free_slot = i;
    }
    if (free_slot < 0)
        return -1;
    streams[free_slot].h = h;
    streams[free_slot].freq = (float)ci.freq;
    streams[free_slot].speed = 1.0f;
    streams[free_slot].type = 0;
    return free_slot;
}

static int stream_arg(Script *s) { return stream((DWORD)script_int(s)); }

static float seconds(int i)
{
    return (float)bass.to_seconds(streams[i].h, bass.length(streams[i].h, 0));
}

static float progress(int i)
{
    uint64_t len = bass.length(streams[i].h, 0);
    return len && len != (uint64_t)-1 ? (float)((double)bass.position(streams[i].h, 0) / (double)len) : 0.0f;
}

static void set_progress(int i, float p)
{
    uint64_t len = bass.length(streams[i].h, 0);
    if (p < 0)
        p = 0;
    if (p > 1)
        p = 1;
    if (len && len != (uint64_t)-1)
        bass.set_position(streams[i].h, (uint64_t)((double)len * p), 0);
}

static void set_speed(int i, float speed, DWORD ms)
{
    float f = streams[i].freq * (speed > 0.01f ? speed : 0.01f);
    streams[i].speed = speed;
    if (ms)
        bass.slide(streams[i].h, BASS_ATTRIB_FREQ, f, ms);
    else
        bass.set_attr(streams[i].h, BASS_ATTRIB_FREQ, f);
}

/* 2500 is_audio_stream_playing %1d% */
static int8_t __stdcall op_2500(Script *s) { int i = stream_arg(s); script_cond(s, i >= 0 && bass.active(streams[i].h) == 1); return 0; }

/* 2501 %2d% = get_audio_stream_duration %1d%: at its speed */
static int8_t __stdcall op_2501(Script *s)
{
    int i = stream_arg(s);
    float len = i >= 0 ? seconds(i) : 0;
    if (i >= 0)
        len = streams[i].speed > 0 ? len / streams[i].speed : 3.402823e38f;
    store_float(s, len);
    return 0;
}

/* 2502 %2d% = get_audio_stream_speed %1d%, 2503 set_audio_stream_speed %1d% %2d% */
static int8_t __stdcall op_2502(Script *s) { int i = stream_arg(s); store_float(s, i >= 0 ? streams[i].speed : 0); return 0; }
static int8_t __stdcall op_2503(Script *s) { int i = stream_arg(s); float v = script_float(s); if (i >= 0) set_speed(i, v, 0); return 0; }

/* 2504 set_audio_stream_volume_with_transition %1d% volume %2d% time_ms %3d% */
static int8_t __stdcall op_2504(Script *s)
{
    int i = stream_arg(s), ms;
    float v;
    script_collect(s, 2);
    v = ScriptParams[0].f;
    ms = ScriptParams[1].i;
    if (i >= 0)
        bass.slide(streams[i].h, BASS_ATTRIB_VOL, v < 0 ? 0 : v, ms > 0 ? (DWORD)ms : 0);
    return 0;
}

/* 2505 set_audio_stream_speed_with_transition %1d% speed %2d% time_ms %3d% */
static int8_t __stdcall op_2505(Script *s)
{
    int i = stream_arg(s);
    script_collect(s, 2);
    if (i >= 0)
        set_speed(i, ScriptParams[0].f, ScriptParams[1].i > 0 ? (DWORD)ScriptParams[1].i : 0);
    return 0;
}

/* 2506 set_audio_stream_source_size %1d% radius %2d%: the 3D min distance */
static int8_t __stdcall op_2506(Script *s)
{
    int i = stream_arg(s);
    float r = script_float(s);
    if (i >= 0) {
        bass.set3d(streams[i].h, -1, r, -1.0f, -1, -1, -1.0f);
        bass.apply3d();
    }
    return 0;
}

/* 2507 / 2508 get / set_audio_stream_progress %1d% (0 to 1) */
static int8_t __stdcall op_2507(Script *s) { int i = stream_arg(s); store_float(s, i >= 0 ? progress(i) : 0); return 0; }
static int8_t __stdcall op_2508(Script *s) { int i = stream_arg(s); float p = script_float(s); if (i >= 0) set_progress(i, p); return 0; }

/* 2509 / 250A get / set_audio_stream_type %1d%: kept, MoonLoader sets the volume */
static int8_t __stdcall op_2509(Script *s) { int i = stream_arg(s); store_int(s, i >= 0 ? streams[i].type : 0); return 0; }
static int8_t __stdcall op_250A(Script *s) { int i = stream_arg(s); int t = script_int(s); if (i >= 0) streams[i].type = t; return 0; }

/* 250B / 250C get / set_audio_stream_progress_seconds %1d% */
static int8_t __stdcall op_250B(Script *s) { int i = stream_arg(s); store_float(s, i >= 0 ? progress(i) * seconds(i) : 0); return 0; }
static int8_t __stdcall op_250C(Script *s)
{
    int i = stream_arg(s);
    float sec = script_float(s), len;
    if (i >= 0) {
        len = seconds(i);
        set_progress(i, len > 0 ? sec / len : 0);
    }
    return 0;
}

/* ---- text ------------------------------------------------------------------- */

static int8_t text_test(Script *s, int how)
{
    char a[512], b[512];
    const char *x = script_string(s, a, sizeof a), *y = script_string(s, b, sizeof b);
    int nocase = script_int(s), nx = lstrlenA(x), ny = lstrlenA(y), r;
    switch (how) {
    case 0:  r = !text_cmp(x, y, nocase, 0x7FFFFFFF); break;
    case 1:  r = text_find(x, y, nocase) != NULL; break;
    case 2:  r = ny <= nx && !text_cmp(x, y, nocase, ny); break;
    default: r = ny <= nx && !text_cmp(x + nx - ny, y, nocase, ny); break;
    }
    script_cond(s, r);
    return 0;
}

/* 2600 is_text_empty %1s% */
static int8_t __stdcall op_2600(Script *s)
{
    char buf[8];
    script_cond(s, !*script_string(s, buf, sizeof buf));
    return 0;
}

/* 2601 is_text_equal, 2602 is_text_in_text, 2603 is_text_prefix, 2604
 * is_text_suffix %1s% %2s% ignore_case %3d% */
static int8_t __stdcall op_2601(Script *s) { return text_test(s, 0); }
static int8_t __stdcall op_2602(Script *s) { return text_test(s, 1); }
static int8_t __stdcall op_2603(Script *s) { return text_test(s, 2); }
static int8_t __stdcall op_2604(Script *s) { return text_test(s, 3); }

/* 2605 display_text_formatted %1d% %2d% %3s% ...: as the next intro text
 * line this frame, under a label of its own per line */
static int8_t __stdcall op_2605(Script *s)
{
    char text[512], label[8];
    uint8_t *line;
    float x, y;
    int n = INTRO_TEXT_COUNT;
    script_collect(s, 2);
    x = ScriptParams[0].f;
    y = ScriptParams[1].f;
    read_formatted(s, text, sizeof text);
    if (n >= 96)
        return 0;
    wsprintfA(label, "\x01MS%02d", n);
    if (!fxt_add(label, text))
        return 0;
    line = INTRO_TEXTS + n * 0x44;
    AT(line, 0x2C, float) = x;
    AT(line, 0x30, float) = y;
    memcpy(line + 0x34, label, 8);
    INTRO_TEXT_COUNT = (uint16_t)(n + 1);
    return 0;
}

/* 2606 load_fxt %1s%, 2607 unload_fxt %1s% */
static int8_t __stdcall op_2606(Script *s)
{
    char path[MAX_PATH];
    read_path(s, path, sizeof path);
    script_cond(s, fxt_file(path, 0) > 0);
    return 0;
}

static int8_t __stdcall op_2607(Script *s)
{
    char path[MAX_PATH];
    read_path(s, path, sizeof path);
    script_cond(s, fxt_file(path, 1) > 0);
    return 0;
}

/* 2608 %2d% = get_text_length %1s% */
static int8_t __stdcall op_2608(Script *s)
{
    char buf[512];
    store_int(s, lstrlenA(script_string(s, buf, sizeof buf)));
    return 0;
}

/* 2609 add_text_label_formatted %1s% %2s% ... */
static int8_t __stdcall op_2609(Script *s)
{
    char key[8], text[512];
    script_string(s, key, sizeof key);
    read_formatted(s, text, sizeof text);
    fxt_add(key, text);
    return 0;
}

/* ---- math ------------------------------------------------------------------- */

static uint32_t rng;

static uint32_t next_random(void)
{
    if (!rng) {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        rng = (uint32_t)t.LowPart ^ GetTickCount() ^ 0x9E3779B9u;
    }
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

/* 2700 is_bit_set %1d% bit %2d% */
static int8_t __stdcall op_2700(Script *s)
{
    script_collect(s, 2);
    script_cond(s, (unsigned)ScriptParams[1].i < 32 && (ScriptParams[0].u >> ScriptParams[1].i) & 1);
    return 0;
}

/* 2701 set_bit, 2702 clear_bit %1d% bit %2d%, 2703 toggle_bit %1d% bit %2d% state %3d% */
static int8_t bit(Script *s, int how)
{
    int32_t *var = script_var(s);
    int b = script_int(s), on = how == 2 ? script_int(s) : how == 0;
    if (var && (unsigned)b < 32) {
        if (on)
            *var |= (int32_t)(1u << b);
        else
            *var &= (int32_t)~(1u << b);
    }
    return 0;
}
static int8_t __stdcall op_2701(Script *s) { return bit(s, 0); }
static int8_t __stdcall op_2702(Script *s) { return bit(s, 1); }
static int8_t __stdcall op_2703(Script *s) { return bit(s, 2); }

/* 2704 is_truthy %1d%: a non-zero number, or text that is not empty */
static int8_t __stdcall op_2704(Script *s)
{
    char buf[8];
    if (is_text(s))
        script_cond(s, *script_string(s, buf, sizeof buf) != 0);
    else
        script_cond(s, script_int(s) != 0);
    return 0;
}

/* 2705 pick_random_int, 2706 pick_random_float, 2707 pick_random_text
 * values... %n%: one of the values into the last argument, a variable */
static int8_t pick(Script *s, int text)
{
    static char picked[256];
    static struct { int32_t *var, value; char str[64]; } arg[33];
    int n = 0, i;
    while (*s->ip) {
        int k = n < 33 ? n++ : 32;
        uint8_t t = *s->ip;
        arg[k].var = NULL;
        arg[k].str[0] = 0;
        if (t == 0x02 || t == 0x03) {
            arg[k].var = script_var(s);
            arg[k].value = arg[k].var ? *arg[k].var : 0;
            if (text && arg[k].value)
                lstrcpynA(arg[k].str, (const char *)(uintptr_t)arg[k].value, sizeof arg[k].str);
        } else if (text) {
            script_string(s, arg[k].str, sizeof arg[k].str);
        } else {
            arg[k].value = script_int(s);
        }
    }
    s->ip++;
    if (n < 2 || !arg[n - 1].var)
        return 0;
    i = (int)(next_random() % (uint32_t)(n - 1));
    if (text) {
        lstrcpynA(picked, arg[i].str, sizeof picked);
        *arg[n - 1].var = (int32_t)(uintptr_t)picked;
    } else {
        *arg[n - 1].var = arg[i].value;
    }
    return 0;
}
static int8_t __stdcall op_2705(Script *s) { return pick(s, 0); }
static int8_t __stdcall op_2706(Script *s) { return pick(s, 0); }
static int8_t __stdcall op_2707(Script *s) { return pick(s, 1); }

/* 2708 random_chance %1d% (percent) */
static int8_t __stdcall op_2708(Script *s)
{
    float chance = script_float(s);
    script_cond(s, (float)(next_random() % 100000) / 1000.0f < chance);
    return 0;
}

/* ---- INI files -------------------------------------------------------------- */

/* 2800 delete_section_from_ini_file %1s% section %2s%, 2801
 * delete_key_from_ini_file %1s% section %2s% key %3s% */
static int8_t ini_delete(Script *s, int key)
{
    char path[MAX_PATH], section[256], name[256] = "";
    read_path(s, path, sizeof path);
    script_string(s, section, sizeof section);
    if (key)
        script_string(s, name, sizeof name);
    script_cond(s, GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES &&
                       WritePrivateProfileStringA(section, key ? name : NULL, NULL, path));
    return 0;
}
static int8_t __stdcall op_2800(Script *s) { return ini_delete(s, 0); }
static int8_t __stdcall op_2801(Script *s) { return ini_delete(s, 1); }

/* ---- core ------------------------------------------------------------------- */

/* 0DD5 %1d% = get_platform: PLATFORM_WINDOWS */
static int8_t __stdcall op_0DD5(Script *s) { store_int(s, 9); return 0; }

/* 2000 %1d% = get_cleo_arg_count: no CLEO function call is running */
static int8_t __stdcall op_2000(Script *s) { store_int(s, 0); return 0; }

/* 2002 cleo_return_with %1d% ..., 2003 cleo_return_fail ...: the condition;
 * the values have no CLEO function call to go back to */
static int8_t __stdcall op_2002(Script *s)
{
    int r = script_int(s);
    script_skip_varargs(s);
    script_cond(s, r);
    return 0;
}

static int8_t __stdcall op_2003(Script *s)
{
    script_skip_varargs(s);
    script_cond(s, 0);
    return 0;
}

void cleo5_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0DD5, op_0DD5 }, { 0x2000, op_2000 }, { 0x2002, op_2002 }, { 0x2003, op_2003 },
        { 0x2080, op_2080 }, { 0x2081, op_2081 }, { 0x2082, op_2082 }, { 0x2083, op_2083 },
        { 0x2084, op_2084 }, { 0x2085, op_2085 }, { 0x2086, op_2086 },
        { 0x2100, op_2100 }, { 0x2101, op_2101 }, { 0x2102, op_2102 },
        { 0x0A9A, op_0A9A }, { 0x0A9B, op_0A9B }, { 0x0A9C, op_0A9C },
        { 0x2300, op_2300 }, { 0x2301, op_2301 }, { 0x2302, op_2302 }, { 0x2303, op_2303 },
        { 0x2304, op_2304 }, { 0x2305, op_2305 },
        { 0x2400, op_2400 }, { 0x2401, op_2401 }, { 0x2402, op_2402 }, { 0x2403, op_2403 },
        { 0x2404, op_2404 }, { 0x2405, op_2405 }, { 0x2406, op_2406 }, { 0x2407, op_2407 },
        { 0x2408, op_2408 },
        { 0x2500, op_2500 }, { 0x2501, op_2501 }, { 0x2502, op_2502 }, { 0x2503, op_2503 },
        { 0x2504, op_2504 }, { 0x2505, op_2505 }, { 0x2506, op_2506 }, { 0x2507, op_2507 },
        { 0x2508, op_2508 }, { 0x2509, op_2509 }, { 0x250A, op_250A }, { 0x250B, op_250B },
        { 0x250C, op_250C },
        { 0x2600, op_2600 }, { 0x2601, op_2601 }, { 0x2602, op_2602 }, { 0x2603, op_2603 },
        { 0x2604, op_2604 }, { 0x2605, op_2605 }, { 0x2606, op_2606 }, { 0x2607, op_2607 },
        { 0x2608, op_2608 }, { 0x2609, op_2609 },
        { 0x2700, op_2700 }, { 0x2701, op_2701 }, { 0x2702, op_2702 }, { 0x2703, op_2703 },
        { 0x2704, op_2704 }, { 0x2705, op_2705 }, { 0x2706, op_2706 }, { 0x2707, op_2707 },
        { 0x2708, op_2708 },
        { 0x2800, op_2800 }, { 0x2801, op_2801 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        if (script_cleo_present())
            script_keep(ops[i].op, ops[i].fn);
        else
            script_register(ops[i].op, ops[i].fn);
    }
}
