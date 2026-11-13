/* SAMPFUNCS's console and global variables.
 *
 * The console opens with ~ over the top left of the screen. It shows
 * moonloader.log as MoonLoader writes it (MoonLoader.asi's WriteFile, so it
 * needs no script) and what scripts send with sampfuncsLog; scriptHelper's
 * copies of moonloader.log ("[ML] ...") are left out. Typed lines run
 * console commands in the frame tick; help, clear and debug are its own.
 * Global variables are shared by every script, all of which may read and
 * write them (the mods share flags and epochs this way). */
#include "moonsamp.h"

#define MAX_CONSOLE_COMMANDS 64
#define MAX_VARS             256
#define MAX_LINES            1000
#define LINE_SIZE            256
#define MAX_ROWS             32         /* rows one line may wrap to */
#define HISTORY              32
#define QUEUE                8
#define MARGIN               10

#define COLOR_BG     0xE0101010u
#define COLOR_EDGE   0xFF505050u
#define COLOR_TEXT   0xFFC0C0C0u
#define COLOR_INPUT  0xFFFFFFFFu
#define COLOR_ECHO   "{808080}"
#define COLOR_ERROR  "{FF7070}"

typedef BOOL (WINAPI *WriteFileFn)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef size_t (__cdecl *FwriteFn)(const void *, size_t, size_t, void *);
typedef int (__cdecl *FilenoFn)(void *);
typedef intptr_t (__cdecl *OsHandleFn)(int);

static struct {
    char name[33];
    char description[128];
    Script *script;
    int32_t label;
} commands[MAX_CONSOLE_COMMANDS];

static struct {
    char name[64];
    int32_t value;
} vars[MAX_VARS];

/* Lines may come from other threads (MoonLoader's log): lock guards them. */
static char lines[MAX_LINES][LINE_SIZE];
static int first_line, line_count, scroll;     /* scroll: rows up from the newest */
static CRITICAL_SECTION lock;
static int visible, show_debug, eat_char, set_cursor;
static EditLine input;
static char history[HISTORY][256];
static int history_count, history_at;
static char queued[QUEUE][256];
static int queue_count;

static WriteFileFn orig_write;
static HANDLE log_file, other_files[8];
static FwriteFn orig_fwrite;
static FilenoFn crt_fileno;
static OsHandleFn crt_oshandle;
static void *log_stream, *other_streams[8];
static char partial[LINE_SIZE];
static int partial_len, hooked;

static int find_command(const char *name)
{
    int i;
    for (i = 0; i < MAX_CONSOLE_COMMANDS; i++)
        if (commands[i].script && lstrcmpiA(commands[i].name, name) == 0)
            return i;
    return -1;
}

static int find_var(const char *name)
{
    int i;
    for (i = 0; i < MAX_VARS; i++)
        if (vars[i].name[0] && lstrcmpA(vars[i].name, name) == 0)
            return i;
    return -1;
}

static const char *arg_string(Script *s, char *buf, size_t size)
{
    copy_string(buf, script_string(s, buf, size), size);
    return buf;
}

/* ---- lines -------------------------------------------------------------- */

void console_print(const char *line)
{
    char *dst;
    int i, j = 0;
    EnterCriticalSection(&lock);
    if (line_count == MAX_LINES)
        first_line = (first_line + 1) % MAX_LINES;
    else
        line_count++;
    dst = lines[(first_line + line_count - 1) % MAX_LINES];
    for (i = 0; line[i] && line[i] != '\r' && line[i] != '\n' && j < LINE_SIZE - 1; i++)
        dst[j++] = line[i] == '\t' ? ' ' : line[i];
    dst[j] = 0;
    if (scroll)
        scroll++;
    LeaveCriticalSection(&lock);
}

static void printf_line(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(buf, fmt, ap);
    va_end(ap);
    console_print(buf);
}

/* "[12:00:00.123456] (type)\tscript: text" as MoonLoader writes it, coloured
 * as scriptHelper colours it, without the debug lines unless asked. */
static void log_line(const char *s)
{
    static const struct { const char *type, *color; } types[] = {
        { "info", "A9EFF5" }, { "debug", "AFA9F5" }, { "error", "FF7070" }, { "warn", "F5C28E" },
        { "system", "FA9746" }, { "fatal", "FF4040" }, { "exception", "F5A9A9" }, { "script", "7DD156" },
    };
    char time[16] = "", type[16] = "";
    const char *color = "C0C0C0", *p = s;
    int n = 0, i;
    if (*p == '[') {
        for (p++; *p && *p != ']'; p++)
            if (n < 12)
                time[n++] = *p;
        time[n] = 0;
        if (*p)
            p++;
    }
    while (*p == ' ')
        p++;
    if (*p == '(') {
        for (n = 0, p++; *p && *p != ')'; p++)
            if (n < (int)sizeof type - 1)
                type[n++] = *p;
        type[n] = 0;
        if (*p)
            p++;
    }
    while (*p == ' ' || *p == '\t')
        p++;
    if (!*p || (!show_debug && lstrcmpA(type, "debug") == 0))
        return;
    for (i = 0; i < (int)(sizeof types / sizeof types[0]); i++)
        if (lstrcmpA(type, types[i].type) == 0)
            color = types[i].color;
    if (type[0])
        printf_line("{808080}[%s] {%s}(%s) {C0C0C0}%s", time, color, type, p);
    else
        printf_line("{C0C0C0}%s", p);
}

static void feed(const char *data, DWORD n)
{
    char line[LINE_SIZE];
    DWORD i;
    for (i = 0; i < n; i++) {
        EnterCriticalSection(&lock);
        if (data[i] == '\n') {
            partial[partial_len] = 0;
            lstrcpyA(line, partial);
            partial_len = 0;
        } else if (data[i] != '\r' && partial_len < LINE_SIZE - 1) {
            partial[partial_len++] = data[i];
        }
        LeaveCriticalSection(&lock);
        if (data[i] == '\n')
            log_line(line);
    }
}

static int is_log_path(HANDLE h)
{
    char path[MAX_PATH];
    int n = GetFinalPathNameByHandleA(h, path, sizeof path, 0);
    return n >= 15 && n < (int)sizeof path && lstrcmpiA(path + n - 15, "\\moonloader.log") == 0;
}

static int is_log(HANDLE h)
{
    int i;
    if (h == log_file)
        return 1;
    if (log_file)
        return 0;
    for (i = 0; i < 8; i++)
        if (other_files[i] == h)
            return 0;
    if (is_log_path(h)) {
        log_file = h;
        return 1;
    }
    for (i = 0; i < 8 && other_files[i]; i++)
        ;
    if (i < 8)
        other_files[i] = h;
    return 0;
}

/* The same for a C stream (v.027 writes its log with fwrite). */
static int is_log_stream(void *stream)
{
    int i, fd;
    intptr_t h;
    if (stream == log_stream)
        return 1;
    if (log_stream || !stream)
        return 0;
    for (i = 0; i < 8; i++)
        if (other_streams[i] == stream)
            return 0;
    fd = crt_fileno(stream);
    h = fd >= 0 ? crt_oshandle(fd) : -1;
    if (h != -1 && is_log_path((HANDLE)h)) {
        log_stream = stream;
        return 1;
    }
    for (i = 0; i < 8 && other_streams[i]; i++)
        ;
    if (i < 8)
        other_streams[i] = stream;
    return 0;
}

static size_t __cdecl hook_fwrite(const void *buf, size_t size, size_t count, void *stream)
{
    size_t done = orig_fwrite(buf, size, count, stream);
    if (done && buf && is_log_stream(stream))
        feed(buf, (DWORD)(done * size));
    return done;
}

static BOOL WINAPI hook_write(HANDLE h, LPCVOID buf, DWORD n, LPDWORD written, LPOVERLAPPED ov)
{
    BOOL ok = orig_write(h, buf, n, written, ov);
    if (ok && buf && is_log(h))
        feed(buf, n);
    return ok;
}

/* MoonLoader.asi's import of WriteFile goes through hook_write (v.026), its
 * C runtime's fwrite through hook_fwrite (v.027). */
static int hook_imports(HMODULE m)
{
    uint8_t *b = (uint8_t *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(b + dir->VirtualAddress);
    void *target = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "WriteFile"), *p = (void *)hook_write;
    void *pf = (void *)hook_fwrite;
    int found = 0;
    if (!dir->VirtualAddress || !target)
        return 0;
    orig_write = (WriteFileFn)target;
    for (; imp->Name; imp++) {
        void **slot = (void **)(b + imp->FirstThunk);
        /* an api-ms-win-crt name may not be a module of its own */
        HMODULE crt = GetModuleHandleA((const char *)b + imp->Name), ucrt = GetModuleHandleA("ucrtbase.dll");
        void *fw = crt ? (void *)GetProcAddress(crt, "fwrite") : NULL;
        if (!fw && ucrt)
            fw = (void *)GetProcAddress(crt = ucrt, "fwrite");
        for (; *slot; slot++) {
            if (*slot == target && ms_patch(slot, &p, sizeof p))
                found = 1;
            if (!fw || *slot != fw || orig_fwrite)
                continue;
            crt_fileno = (FilenoFn)(void *)GetProcAddress(crt, "_fileno");
            crt_oshandle = (OsHandleFn)(void *)GetProcAddress(crt, "_get_osfhandle");
            if (!crt_fileno || !crt_oshandle)
                continue;
            orig_fwrite = (FwriteFn)fw;
            if (ms_patch(slot, &pf, sizeof pf))
                found = 1;
        }
    }
    return found;
}

/* What MoonLoader wrote before the hook: the end of moonloader.log. */
static void backfill(HMODULE m)
{
    char path[MAX_PATH], *slash, *buf, *start;
    HANDLE f;
    DWORD size, got = 0;
    GetModuleFileNameA(m, path, MAX_PATH - 32);
    slash = path + lstrlenA(path);
    while (slash > path && slash[-1] != '\\' && slash[-1] != '/')
        slash--;
    lstrcpyA(slash, "moonloader\\moonloader.log");
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    size = GetFileSize(f, NULL);
    if (size != INVALID_FILE_SIZE && size > 0x10000) {
        SetFilePointer(f, size - 0x10000, NULL, FILE_BEGIN);
        size = 0x10000;
    }
    if (size != INVALID_FILE_SIZE && (buf = HeapAlloc(GetProcessHeap(), 0, size))) {
        if (ReadFile(f, buf, size, &got, NULL)) {
            start = buf;
            if (size == 0x10000)
                while (start < buf + got && *start++ != '\n')
                    ;
            feed(start, got - (start - buf));
        }
        HeapFree(GetProcessHeap(), 0, buf);
    }
    CloseHandle(f);
}

/* ---- commands ------------------------------------------------------------ */

static void run_line(const char *line)
{
    char name[33];
    const char *params;
    int n, i;
    for (params = line; *params == ' '; params++)
        ;
    for (n = 0; params[n] && params[n] != ' ' && n < 32; n++)
        name[n] = params[n];
    name[n] = 0;
    params += n;
    while (*params == ' ')
        params++;
    if (!name[0])
        return;
    if ((i = find_command(name)) >= 0) {
        samp_run_command(commands[i].script, commands[i].label, params);
    } else if (lstrcmpiA(name, "help") == 0 || lstrcmpiA(name, "cmdlist") == 0) {
        console_print("{FFFFFF}help, clear, debug, and the scripts' commands:");
        for (i = 0; i < MAX_CONSOLE_COMMANDS; i++)
            if (commands[i].script)
                printf_line("  {FFFF00}%s{C0C0C0}%s%s", commands[i].name, commands[i].description[0] ? " - " : "",
                            commands[i].description);
    } else if (lstrcmpiA(name, "clear") == 0) {
        EnterCriticalSection(&lock);
        line_count = scroll = 0;
        LeaveCriticalSection(&lock);
    } else if (lstrcmpiA(name, "debug") == 0) {
        show_debug = !show_debug;
        printf_line("MoonLoader's debug lines are %s", show_debug ? "shown" : "hidden");
    } else {
        printf_line(COLOR_ERROR "Unknown command: %s", name);
        ms_log("console: unknown command \"%s\"", name);
    }
}

/* Once a frame: hooks MoonLoader's log once it is loaded, runs typed lines. */
void console_tick(void)
{
    HMODULE m;
    int i, n;
    if (!hooked && (m = GetModuleHandleA("MoonLoader.asi"))) {
        hooked = 1;
        if (hook_imports(m))
            backfill(m);
        else
            ms_log("console: MoonLoader.asi imports no WriteFile or fwrite, its log is not shown");
    }
    n = queue_count;
    queue_count = 0;
    for (i = 0; i < n; i++)
        run_line(queued[i]);
}

/* Tab: the one command starting with what is typed, or the list of them. */
static void complete(void)
{
    char found[40] = "";
    int i, matches = 0, n = input.caret;
    for (i = 0; n && i < MAX_CONSOLE_COMMANDS; i++)
        if (commands[i].script && lstrlenA(commands[i].name) >= n &&
            CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, commands[i].name, n, input.text, n) == CSTR_EQUAL) {
            if (matches++ == 0)
                lstrcpyA(found, commands[i].name);
            else
                printf_line("  %s", commands[i].name);
        }
    if (matches == 1) {
        lstrcatA(found, " ");
        edit_set(&input, found);
    } else if (matches > 1) {
        printf_line("  %s", found);
    }
}

/* ---- window ------------------------------------------------------------- */

int console_visible(void) { return visible; }

/* Open, it shows SA-MP's cursor (which holds the camera and controls) if it
 * is not already shown. */
static void show(int on)
{
    visible = on;
    if (on && samp_cursor_mode() == 0) {
        samp_set_cursor_mode(3);
        set_cursor = 1;
    } else if (!on && set_cursor) {
        set_cursor = 0;
        if (samp_cursor_mode() == 3)
            samp_set_cursor_mode(0);
    }
}

/* The window's rectangle; returns a row's height. */
static int layout(int *x, int *y, int *w, int *h)
{
    int sw = overlay_width(), sh = overlay_height();
    *x = *y = MARGIN;
    *w = sw * 3 / 5 < 640 ? sw - 2 * MARGIN : sw * 3 / 5;
    *h = sh * 9 / 20;
    return overlay_line_height(FONT_CONSOLE) + 1;
}

/* How many characters of s fit in w pixels, never cutting a colour code. */
static int fit(const char *s, int w)
{
    int lo = 1, hi = lstrlenA(s), n, k;
    if (overlay_text_width(FONT_CONSOLE, s, hi) <= w)
        return hi;
    while (lo < hi) {
        n = (lo + hi + 1) / 2;
        if (overlay_text_width(FONT_CONSOLE, s, n) <= w)
            lo = n;
        else
            hi = n - 1;
    }
    for (k = lo - 1; k > 0 && k > lo - 8; k--)
        if (s[k] == '{') {
            lo = k;
            break;
        }
    return lo;
}

/* Where a line's rows start in w pixels; returns how many rows. */
static int rows_of(const char *s, int w, int *starts)
{
    int n = 0, at = 0, len = lstrlenA(s);
    do {
        starts[n++] = at;
        at += fit(s + at, w);
    } while (at < len && n < MAX_ROWS);
    return n;
}

void console_render(void)
{
    int x, y, w, h, rh, tw, pw, rows, row = 0, skip, i, r, n;
    int starts[MAX_ROWS];
    char text[LINE_SIZE];
    if (!visible)
        return;
    rh = layout(&x, &y, &w, &h);
    tw = w - 12;
    rows = (h - rh - 12) / rh;
    pw = overlay_text_width(FONT_CONSOLE, "> ", -1);
    overlay_rect(x, y, w, h, COLOR_BG);
    overlay_frame(x, y, w, h, COLOR_EDGE);
    overlay_rect(x + 1, y + h - rh - 6, w - 2, 1, COLOR_EDGE);
    overlay_text(FONT_CONSOLE | TEXT_RAW, x + 6, y + h - rh - 3, ">", COLOR_INPUT, 0);
    edit_draw(&input, FONT_CONSOLE, x + 6 + pw, y + h - rh - 3, w - 12 - pw, COLOR_INPUT, 1);
    EnterCriticalSection(&lock);
    skip = scroll;
    for (i = line_count - 1; i >= 0 && row < rows; i--) {
        const char *s = lines[(first_line + i) % MAX_LINES];
        n = rows_of(s, tw, starts);
        for (r = n - 1; r >= 0 && row < rows; r--) {
            if (skip) {
                skip--;
                continue;
            }
            lstrcpynA(text, s + starts[r], (r + 1 < n ? starts[r + 1] : lstrlenA(s)) - starts[r] + 1);
            overlay_text(FONT_CONSOLE, x + 6, y + 4 + (rows - 1 - row) * rh, text,
                         overlay_color_at(s, starts[r], COLOR_TEXT), tw);
            row++;
        }
    }
    if (i < 0 && skip)
        scroll -= skip;
    LeaveCriticalSection(&lock);
    if (scroll)
        overlay_text(FONT_CONSOLE, x + w - 6 - overlay_text_width(FONT_CONSOLE, "[scrolled]", -1), y + 4,
                     "{FFFF00}[scrolled]", COLOR_TEXT, 0);
}

static void submit(void)
{
    if (!input.len)
        return;
    printf_line(COLOR_ECHO "> %s", input.text);
    if (!history_count || lstrcmpA(history[(history_count - 1) % HISTORY], input.text) != 0)
        lstrcpyA(history[history_count++ % HISTORY], input.text);
    history_at = history_count;
    if (queue_count < QUEUE)
        lstrcpyA(queued[queue_count++], input.text);
    edit_set(&input, "");
    scroll = 0;
}

static void recall(int dir)
{
    int oldest = history_count > HISTORY ? history_count - HISTORY : 0;
    if (!history_count)
        return;
    history_at += dir;
    if (history_at < oldest)
        history_at = oldest;
    if (history_at >= history_count) {
        history_at = history_count;
        edit_set(&input, "");
        return;
    }
    edit_set(&input, history[history_at % HISTORY]);
}

int console_input(UINT msg, WPARAM wp, LPARAM lp)
{
    int x, y, w, h, rh, page, mx, my;
    if (msg == WM_KEYDOWN && wp == VK_OEM_3 && (visible || !samp_typing())) {
        show(!visible);
        eat_char = 1;
        return 1;
    }
    if (msg == WM_CHAR && eat_char) {
        eat_char = 0;
        return 1;
    }
    if (!visible)
        return 0;
    rh = layout(&x, &y, &w, &h);
    page = (h - rh - 12) / rh - 1;
    if (msg == WM_KEYDOWN) {
        switch (wp) {
        case VK_ESCAPE: show(0); eat_char = 1; return 1;
        case VK_UP:     recall(-1); return 1;
        case VK_DOWN:   recall(1); return 1;
        case VK_PRIOR:  scroll += page; return 1;
        case VK_NEXT:   scroll = scroll > page ? scroll - page : 0; return 1;
        case VK_TAB:    complete(); eat_char = 1; return 1;
        }
    }
    if (msg == WM_CHAR && wp == '\r') {
        submit();
        return 1;
    }
    if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR) {
        edit_key(&input, msg, wp);
        return 1;
    }
    if (msg == WM_MOUSEWHEEL) {
        scroll += (short)HIWORD(wp) / WHEEL_DELTA * 3;
        if (scroll < 0)
            scroll = 0;
        return 1;
    }
    if (msg >= WM_LBUTTONDOWN && msg <= WM_MOUSELAST) {
        overlay_point(msg, lp, &mx, &my);
        return mx >= x && my >= y && mx < x + w && my < y + h;
    }
    return 0;
}

/* ---- opcodes ------------------------------------------------------------ */

/* 0B78 log %1s% ...: to the console, and to moonsamp.log without colour
 * codes. scriptHelper's copies of moonloader.log ("[ML] ...") are left out:
 * the console shows moonloader.log itself. */
static int8_t __stdcall op_0B78(Script *s)
{
    char text[512], out[512];
    int i, j = 0;
    arg_string(s, text, sizeof text);
    script_skip_varargs(s);
    for (i = 0; text[i] == '{' && lstrlenA(text + i) >= 8 && text[i + 7] == '}'; i += 8)
        ;
    if (text[i] == '[' && text[i + 1] == 'M' && text[i + 2] == 'L' && text[i + 3] == ']')
        return 0;
    console_print(text);
    for (i = 0; text[i] && j < (int)sizeof out - 1; i++) {
        if (text[i] == '{' && lstrlenA(text + i) >= 8 && text[i + 7] == '}') {
            i += 7;
            continue;
        }
        out[j++] = text[i];
    }
    out[j] = 0;
    ms_log("console: %s", out);
    return 0;
}

/* 0C7E is_console_active */
static int8_t __stdcall op_0C7E(Script *s)
{
    script_cond(s, visible);
    return 0;
}

/* 0C63 register_console_command %1d% to_label %2p% */
static int8_t __stdcall op_0C63(Script *s)
{
    char name[64];
    int32_t label;
    int i;
    arg_string(s, name, sizeof name);
    label = script_int(s);
    if (!name[0] || lstrlenA(name) > 32 || find_command(name) >= 0) {
        script_cond(s, 0);
        return 0;
    }
    for (i = 0; i < MAX_CONSOLE_COMMANDS && commands[i].script; i++)
        ;
    if (i < MAX_CONSOLE_COMMANDS) {
        copy_string(commands[i].name, name, sizeof commands[i].name);
        commands[i].description[0] = 0;
        commands[i].label = label;
        commands[i].script = s;
    }
    script_cond(s, i < MAX_CONSOLE_COMMANDS);
    return 0;
}

/* 0C64 unregister_console_command %1d% */
static int8_t __stdcall op_0C64(Script *s)
{
    char name[64];
    int i = find_command(arg_string(s, name, sizeof name));
    if (i >= 0)
        commands[i].script = NULL;
    script_cond(s, i >= 0);
    return 0;
}

/* 0C91 is_console_command_defined %1d% */
static int8_t __stdcall op_0C91(Script *s)
{
    char name[64];
    script_cond(s, find_command(arg_string(s, name, sizeof name)) >= 0);
    return 0;
}

/* 0C62 exec_console_command %1d%: "name params" */
static int8_t __stdcall op_0C62(Script *s)
{
    char line[512];
    run_line(arg_string(s, line, sizeof line));
    return 0;
}

/* 0C7F samp set_client_command %1d% description %2d%: SAMPFUNCS shows chat
 * command descriptions only in its own command list */
static int8_t __stdcall op_0C7F(Script *s)
{
    char a[64], b[256];
    arg_string(s, a, sizeof a);
    arg_string(s, b, sizeof b);
    return 0;
}

/* 0C80 set_console_command %1d% description %2d%: help shows it */
static int8_t __stdcall op_0C80(Script *s)
{
    char name[64], text[256];
    int i = find_command(arg_string(s, name, sizeof name));
    arg_string(s, text, sizeof text);
    if (i >= 0)
        copy_string(commands[i].description, text, sizeof commands[i].description);
    return 0;
}

/* 0BFC set_global_var %1d% = %2d% */
static int8_t __stdcall op_0BFC(Script *s)
{
    char name[64];
    int i;
    int32_t v;
    arg_string(s, name, sizeof name);
    v = script_int(s);
    i = find_var(name);
    if (i < 0 && name[0])
        for (i = 0; i < MAX_VARS && vars[i].name[0]; i++)
            ;
    if (i >= 0 && i < MAX_VARS) {
        copy_string(vars[i].name, name, sizeof vars[i].name);
        vars[i].value = v;
    }
    script_cond(s, i >= 0 && i < MAX_VARS);
    return 0;
}

/* 0BFD %2d% = get_global_var %1d% */
static int8_t __stdcall op_0BFD(Script *s)
{
    char name[64];
    int i = find_var(arg_string(s, name, sizeof name));
    ScriptParams[0].i = i >= 0 ? vars[i].value : 0;
    script_store(s, 1);
    script_cond(s, i >= 0);
    return 0;
}

/* 0C5F is_global_var %1d% defined */
static int8_t __stdcall op_0C5F(Script *s)
{
    char name[64];
    script_cond(s, find_var(arg_string(s, name, sizeof name)) >= 0);
    return 0;
}

/* 0C61 get_global_var %1d% access_for_thread %2d% read_to %3d% write_to %4d% */
static int8_t __stdcall op_0C61(Script *s)
{
    char name[64];
    int defined = find_var(arg_string(s, name, sizeof name)) >= 0;
    script_int(s);
    ScriptParams[0].i = defined;
    ScriptParams[1].i = defined;
    script_store(s, 2);
    return 0;
}

/* 0C60 set_global_var %1d% access_for_thread %2d% read %3d% write %4d%:
 * everyone may already read and write. */
static int8_t __stdcall op_0C60(Script *s)
{
    char name[64];
    arg_string(s, name, sizeof name);
    script_collect(s, 3);
    script_cond(s, find_var(name) >= 0);
    return 0;
}

void console_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B78, op_0B78 }, { 0x0C7E, op_0C7E }, { 0x0C63, op_0C63 }, { 0x0C64, op_0C64 },
        { 0x0C91, op_0C91 }, { 0x0C62, op_0C62 }, { 0x0C7F, op_0C7F }, { 0x0C80, op_0C80 },
        { 0x0BFC, op_0BFC }, { 0x0BFD, op_0BFD }, { 0x0C5F, op_0C5F }, { 0x0C61, op_0C61 },
        { 0x0C60, op_0C60 },
    };
    size_t i;
    InitializeCriticalSection(&lock);
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
