/* moonsamp: the SA-MP API for MoonLoader without SAMPFUNCS or CLEO.
 *
 * MoonLoader's samp* and raknet* functions are SAMPFUNCS opcodes. This ASI
 * registers the same opcodes, so scripts run unchanged. It stays out of the
 * way when SAMPFUNCS.asi is installed. */
#include <stdarg.h>
#include "moonsamp.h"

#define GAME_DIR_MAX MAX_PATH

static char game_dir[GAME_DIR_MAX];
static HANDLE log_file = INVALID_HANDLE_VALUE;

/* moonsamp.log never grows past LOG_MAX: a line that would cross it starts
 * the file over with its first lines (version, SA-MP build, hooks) kept,
 * so a long traced session cannot fill the disk. */
#define LOG_MAX  (1024 * 1024)
#define LOG_HEAD 2048
static char log_head[LOG_HEAD];
static DWORD log_head_len, log_size;
static int log_head_full;

void ms_log(const char *fmt, ...)
{
    static const char cut[] = "--- moonsamp.log reached 1 MB: older lines dropped ---\r\n";
    char buf[1100];
    SYSTEMTIME t;
    DWORD w;
    int n;
    va_list ap;
    if (log_file == INVALID_HANDLE_VALUE)
        return;
    GetLocalTime(&t);
    n = wsprintfA(buf, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_start(ap, fmt);
    n += wvsprintfA(buf + n, fmt, ap);
    va_end(ap);
    buf[n++] = '\r';
    buf[n++] = '\n';
    if (!log_head_full) {
        if (log_head_len + n <= LOG_HEAD) {
            memcpy(log_head + log_head_len, buf, n);
            log_head_len += n;
        } else {
            log_head_full = 1;
        }
    }
    if (log_size + n > LOG_MAX) {
        SetFilePointer(log_file, 0, NULL, FILE_BEGIN);
        SetEndOfFile(log_file);
        WriteFile(log_file, log_head, log_head_len, &w, NULL);
        WriteFile(log_file, cut, sizeof cut - 1, &w, NULL);
        log_size = log_head_len + sizeof cut - 1;
    }
    WriteFile(log_file, buf, n, &w, NULL);
    log_size += n;
}

/* No CRT: gcc still emits calls to these for struct copies. */
void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *a = d;
    const uint8_t *b = s;
    while (n--)
        *a++ = *b++;
    return d;
}

void *memset(void *d, int c, size_t n)
{
    uint8_t *a = d;
    while (n--)
        *a++ = (uint8_t)c;
    return d;
}

int memcmp(const void *x, const void *y, size_t n)
{
    const uint8_t *a = x, *b = y;
    for (; n; n--, a++, b++)
        if (*a != *b)
            return *a - *b;
    return 0;
}

int ms_patch(void *addr, const void *bytes, size_t n)
{
    DWORD old;
    if (!VirtualProtect(addr, n, PAGE_EXECUTE_READWRITE, &old)) {
        ms_log("VirtualProtect(%08lX) failed: %lu", (DWORD)addr, GetLastError());
        return 0;
    }
    memcpy(addr, bytes, n);
    VirtualProtect(addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, n);
    return 1;
}

static int game_file_exists(const char *name)
{
    char path[GAME_DIR_MAX + 64];
    wsprintfA(path, "%s%s", game_dir, name);
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

/* MoonLoader registers the samp* functions only when both isCleoLoaded()
 * and isSampfuncsLoaded() are true at startup. Each is "the 6-byte
 * instruction at a hook site starts with a jmp": 0x4667DB for CLEO,
 * 0x561931 (CTimer::Update) for SAMPFUNCS. moonsamp puts a jmp there to a
 * trampoline that runs the moved instruction and returns; the SAMPFUNCS one
 * first calls samp_tick, once a frame. eax and an x87 value are live there:
 * pushad saves the first, and the tick does no floating point. */
static int mark(uint8_t *site, const uint8_t orig[6], void (*tick)(void), const char *what)
{
    uint8_t *tramp, *t, jmp[6];
    if (memcmp(site, orig, 6) != 0) {
        ms_log("%s site %08lX is already patched (%02X)", what, (DWORD)site, (int)site[0]);
        return site[0] == 0xE9;
    }
    tramp = t = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp)
        return 0;
    if (tick) {
        *t++ = 0x60;                                        /* pushad */
        *t++ = 0x9C;                                        /* pushfd */
        *t++ = 0xE8;                                        /* call tick */
        *(int32_t *)t = (int32_t)((uint8_t *)tick - (t + 4));
        t += 4;
        *t++ = 0x9D;                                        /* popfd */
        *t++ = 0x61;                                        /* popad */
    }
    memcpy(t, orig, 6);
    t += 6;
    *t++ = 0xE9;
    *(int32_t *)t = (int32_t)(site + 6 - (t + 4));
    jmp[0] = 0xE9;
    *(int32_t *)(jmp + 1) = (int32_t)(tramp - (site + 5));
    jmp[5] = 0x90;
    return ms_patch(site, jmp, sizeof jmp);
}

static int mark_loaded(void)
{
    static const uint8_t sf[6] = { 0x8B, 0x0D, 0x84, 0xCB, 0xB7, 0x00 };   /* mov ecx, [0xB7CB84] */
    static const uint8_t cleo[6] = { 0x8A, 0x86, 0xDC, 0x00, 0x00, 0x00 }; /* mov al, [esi+0xDC] */
    int ok = mark((uint8_t *)0x561931, sf, samp_tick, "SAMPFUNCS");
    if (!script_cleo_present())
        ok &= mark((uint8_t *)0x4667DB, cleo, NULL, "CLEO");
    return ok;
}

static int game_is_10us(void)
{
    /* CLEO's 1.0 US probe, or the opcode table operand still in place. */
    return *(uint32_t *)0x82457C == 0x94BF ||
           (*(uint8_t *)0x469FEB == 0x8B && *(uint32_t *)0x469FEE == 0x8A6168);
}

static void start(HMODULE self)
{
    char path[GAME_DIR_MAX + 32];
    int i;
    (void)self;
    GetModuleFileNameA(NULL, game_dir, sizeof game_dir);
    for (i = lstrlenA(game_dir); i > 0 && game_dir[i - 1] != '\\' && game_dir[i - 1] != '/'; i--)
        ;
    game_dir[i] = 0;
    wsprintfA(path, "%smoonsamp.log", game_dir);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    ms_log("moonsamp " MOONSAMP_VERSION);

    if (GetModuleHandleA("SAMPFUNCS.asi") || game_file_exists("SAMPFUNCS.asi")) {
        ms_log("SAMPFUNCS.asi is installed: moonsamp stays off");
        return;
    }
    if (!game_is_10us()) {
        ms_log("not GTA:SA 1.0 US: moonsamp stays off");
        return;
    }
    script_trace = game_file_exists("moonsamp.trace");
    if (script_trace)
        ms_log("tracing opcode answers (moonsamp.trace)");
    if (!script_host_install())
        return;
    if (!samp_install())
        return;
    if (!mark_loaded())
        ms_log("could not mark SAMPFUNCS/CLEO as loaded: MoonLoader will not register samp*");
    ms_log("ready");
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        start(inst);
    }
    return TRUE;
}
