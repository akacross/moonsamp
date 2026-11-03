/* moonsamp's opcode host against gta_sa.exe's own script engine, under Wine.
 * Loaded by a copy of gta_sa.exe whose entry point only calls run() (see
 * run.sh); no game code runs but what this calls. Each case writes the
 * parameters the way MoonLoader does (0x101DB370 buffer, local var 0 for
 * results) and calls CommandHandlerTable[op / 100] through 0x469FEE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef int8_t (__attribute__((thiscall)) *Handler)(void *, int);
typedef int8_t (__attribute__((thiscall)) *ProcessOneFn)(void *);

static uint8_t script[0x100];
static uint8_t code[0x400];
static int failed, passed;

#define BASE_IP  (*(uint8_t **)(script + 0x10))
#define IP       (*(uint8_t **)(script + 0x14))
#define LOCAL(i) (*(int32_t *)(script + 0x3C + 4 * (i)))
#define COND     (script[0xC5])

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (ok) passed++; else failed++;
}

static size_t put(size_t n, const void *p, size_t len) { memcpy(code + n, p, len); return n + len; }
static size_t put8(size_t n, uint8_t v) { code[n] = v; return n + 1; }
static size_t put32(size_t n, uint32_t v) { return put(n, &v, 4); }
static size_t put_int(size_t n, int32_t v) { return put32(put8(n, 0x01), (uint32_t)v); }
static size_t put_local0(size_t n) { n = put8(n, 0x03); return put(n, "\0\0", 2); }
static size_t put_str(size_t n, const char *s)
{
    size_t len = strlen(s);
    if (len > 0x7F)
        return put_int(n, (int32_t)(uintptr_t)s);
    n = put8(put8(n, 0x0E), (uint8_t)len);
    return put(n, s, len);
}

static int8_t call(int op, size_t len)
{
    Handler *table = *(Handler **)0x469FEE;
    (void)len;
    BASE_IP = code;
    IP = code;
    script[0xD2] = 0;
    COND = 0xAA;
    return table[op / 100](script, op);
}

static void *got_obj;
static const char *got_params;
static void __cdecl callback(void *obj, const char *params)
{
    got_obj = obj;
    got_params = params;
}

__declspec(dllexport) int run(void)
{
    HMODULE m;
    void **table;
    size_t n;
    int i, same = 1;
    char log[2048] = "";
    FILE *f;

    setvbuf(stdout, NULL, _IONBF, 0);
    m = LoadLibraryA("moonsamp.asi");
    check(m != NULL, "moonsamp.asi loads");
    if (!m)
        return 1;

    table = *(void ***)0x469FEE;
    check(table != (void **)0x8A6168, "0x469FEE points at the extended table");
    check(*(void ***)0x469EF0 == table, "0x469EF0 (ProcessOneCommand) points at it too");
    for (i = 0; i < 27; i++)
        same &= table[i] == ((void **)0x8A6168)[i];
    check(same, "game opcode slots 0-26 are the game's handlers");
    check(*(uint8_t *)0x561931 == 0xE9, "isSampfuncsLoaded marker jmp at 0x561931");
    check(*(uint8_t *)0x4667DB == 0xE9, "isCleoLoaded marker jmp at 0x4667DB (MoonLoader needs both)");
    {
        uint8_t *t = (uint8_t *)0x4667DB + 5 + *(int32_t *)0x4667DC;
        check(memcmp(t, "\x8A\x86\xDC\x00\x00\x00\xE9", 7) == 0 &&
              t + 11 + *(int32_t *)(t + 7) == (uint8_t *)0x4667E1,
              "its trampoline runs mov al,[esi+0xDC] and returns to 0x4667E1");
    }

    /* 0AFA without samp.dll: false */
    call(0x0AFA, 0);
    check(COND == 0, "0AFA is_samp_available is false without samp.dll");

    /* 0B4F gamestate -> local 0 */
    LOCAL(0) = 77;
    n = put_local0(0);
    call(0x0B4F, n);
    check(LOCAL(0) == 0 && IP == code + n, "0B4F stores 0 into local var 0 and consumes it");

    /* 0AF8: short string, color, end of varargs (MoonLoader's layout) */
    n = put_str(0, "hello {FF0000}world");
    n = put_int(n, (int32_t)0xFFFFFFFF);
    n = put8(n, 0x00);
    call(0x0AF8, n);
    check(IP == code + n, "0AF8 consumes a 0x0E string, an int and the vararg end");

    /* 0AF9: string over 127 bytes goes as a char* in an int */
    {
        static char longtext[200];
        memset(longtext, 'x', 150);
        n = put_str(0, longtext);
        n = put8(n, 0x00);
        call(0x0AF9, n);
        check(IP == code + n, "0AF9 consumes a long string passed as a pointer");
    }

    /* commands before samp.dll exists: kept, then visible to 0C90 */
    n = put_str(0, "mscmd");
    n = put_int(n, -2);
    call(0x0B34, n);
    check(COND == 1 && IP == code + n, "0B34 registers a command before SA-MP is up");
    n = put_str(0, "MSCMD");
    call(0x0C90, n);
    check(COND == 1, "0C90 finds it, case-insensitive");
    n = put_str(0, "mscmd");
    n = put_int(n, -2);
    call(0x0B34, n);
    check(COND == 0, "0B34 refuses the same name twice");
    n = put_str(0, "mscmd");
    call(0x0B63, n);
    check(COND == 1, "0B63 unregisters it");
    n = put_str(0, "mscmd");
    call(0x0C90, n);
    check(COND == 0, "0C90 no longer finds it");

    /* MoonLoader's command stub, byte for byte, run by the game's own
     * ProcessOneCommand: 0B35 -> local 0, 0AA5 cb(obj, params), 0B43. */
    {
        ProcessOneFn step = (ProcessOneFn)0x469EB0;
        uint8_t *end;
        void *obj = (void *)0x12345678;
        n = 0;
        n = put(n, "\x35\x0b", 2); n = put_local0(n);
        n = put(n, "\xa5\x0a", 2); n = put_int(n, (int32_t)(uintptr_t)callback);
        n = put(n, "\x04\x02\x04\x02", 4);
        n = put_local0(n);
        n = put_int(n, (int32_t)(uintptr_t)obj);
        n = put8(n, 0x00);
        n = put(n, "\x43\x0b", 2);
        end = code + n;
        BASE_IP = code;
        IP = code;
        for (i = 0; i < 3; i++)
            step(script);
        check(IP == end, "the command stub runs through 0B35, 0AA5 and 0B43");
        check(got_obj == obj && got_params && got_params[0] == 0,
              "0AA5 calls cb(obj, params) in MoonLoader's argument order");
    }

    /* unknown SAMPFUNCS opcode: false, no crash */
    LOCAL(0) = 0x1234;
    call(0x0B99, 0);
    check(COND == 0, "an unimplemented opcode returns false");
    check(LOCAL(0) != 0x1234 && *(const char *)(uintptr_t)LOCAL(0) == 0,
          "an unimplemented opcode leaves \"\" in its result vars");

    /* phase 4: global variables are shared by every script */
    {
        static uint8_t other[0x100];
        n = put_str(0, "akacross_flag");
        n = put_int(n, 5);
        call(0x0BFC, n);
        check(COND == 1, "0BFC sets a global variable");
        memcpy(other, script, sizeof other);
        n = put_str(0, "akacross_flag");
        n = put_local0(n);
        *(uint8_t **)(other + 0x10) = *(uint8_t **)(other + 0x14) = code;
        other[0xD2] = 0;
        ((Handler)(*(Handler **)0x469FEE)[0x0BFD / 100])(other, 0x0BFD);
        check(other[0xC5] == 1 && *(int32_t *)(other + 0x3C) == 5, "0BFD reads it from another script");
        n = put_str(0, "nope");
        n = put_local0(n);
        LOCAL(0) = 9;
        call(0x0BFD, n);
        check(COND == 0 && LOCAL(0) == 0, "0BFD: an undefined variable is false, 0");
        n = put_str(0, "akacross_flag");
        call(0x0C5F, n);
        check(COND == 1, "0C5F defined");
        n = put_str(0, "akacross_flag");
        n = put_int(n, 0x1234);
        n = put_local0(n);
        n = put8(n, 0x03); n = put(n, "\x01\x00", 2);
        LOCAL(0) = LOCAL(1) = 0;
        call(0x0C61, n);
        check(LOCAL(0) == 1 && LOCAL(1) == 1, "0C61 every script may read and write");
    }

    /* phase 4: console */
    call(0x0C7E, 0);
    check(COND == 0, "0C7E there is no console window");
    n = put_str(0, "{FFFF00}[ML] {C0C0C0}mirrored line");
    n = put8(n, 0x00);
    call(0x0B78, n);
    n = put_str(0, "{FF0000}hello {FFFFFF}world");
    n = put8(n, 0x00);
    call(0x0B78, n);
    check(IP == code + n, "0B78 consumes its string and vararg end");
    {
        /* MoonLoader's command script: the stub at base + 32 */
        static uint8_t cmd_script[0x100], cmd_code[64];
        size_t m = 32;
        void *obj = (void *)0x0BADF00D;
        memcpy(cmd_code + m, "\x35\x0b\x03\x00\x00\xa5\x0a\x01", 8); m += 8;
        *(uint32_t *)(cmd_code + m) = (uint32_t)(uintptr_t)callback; m += 4;
        memcpy(cmd_code + m, "\x04\x02\x04\x02\x03\x00\x00\x01", 8); m += 8;
        *(uint32_t *)(cmd_code + m) = (uint32_t)(uintptr_t)obj; m += 4;
        memcpy(cmd_code + m, "\x00\x43\x0b", 3);
        *(uint8_t **)(cmd_script + 0x10) = *(uint8_t **)(cmd_script + 0x14) = cmd_code;
        n = put_str(0, "lua");
        n = put_int(n, -32);
        memcpy(code + 0x300, code, n);
        *(uint8_t **)(cmd_script + 0x10) = *(uint8_t **)(cmd_script + 0x14) = code + 0x300;
        cmd_script[0xD2] = 0;
        ((Handler)(*(Handler **)0x469FEE)[0x0C63 / 100])(cmd_script, 0x0C63);
        *(uint8_t **)(cmd_script + 0x10) = *(uint8_t **)(cmd_script + 0x14) = cmd_code;
        check(cmd_script[0xC5] == 1, "0C63 registers console command \"lua\"");
        n = put_str(0, "LUA");
        call(0x0C91, n);
        check(COND == 1, "0C91 finds it");
        got_obj = NULL; got_params = NULL;
        n = put_str(0, "lua print(1)");
        call(0x0C62, n);
        check(got_obj == obj && got_params && strcmp(got_params, "print(1)") == 0,
              "0C62 runs its stub with the params \"print(1)\"");
        n = put_str(0, "lua");
        call(0x0C64, n);
        check(COND == 1, "0C64 unregisters it");
        n = put_str(0, "lua x");
        got_obj = NULL;
        call(0x0C62, n);
        check(got_obj == NULL, "0C62 then runs nothing");
    }

    f = fopen("moonsamp.log", "rb");
    if (f) {
        size_t r = fread(log, 1, sizeof log - 1, f);
        log[r] = 0;
        fclose(f);
    }
    check(strstr(log, "opcode table installed") != NULL, "log: opcode table installed");
    check(strstr(log, "opcode 0B99 is not implemented") != NULL, "log: unimplemented opcode named once");
    check(strstr(log, "ready") != NULL, "log: ready");
    check(strstr(log, "console: hello world") != NULL, "log: sampfuncsLog without colour tags");
    check(strstr(log, "mirrored line") == NULL, "log: MoonLoader's own mirrored lines left out");
    check(strstr(log, "console: unknown command \"lua\"") != NULL, "log: unknown console command");

    printf("%d passed, %d failed\n", passed, failed);
    fflush(stdout);
    return failed ? 1 : 0;
}
