/* moonsamp beside a real CLEO.asi (4 or 5) under Wine (see run.sh: CLEO=).
 * CLEO keeps its own opcode table; moonsamp's handlers go through
 * CLEO_RegisterOpcode, MoonLoader must still see both markers, and under
 * CLEO 5 a callback stub must run while its script is inside an opcode. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef int8_t (__attribute__((thiscall)) *Handler)(void *, int);
typedef int (__cdecl *RunFn)(int, const char *, const int32_t *, int, int32_t *);

static uint8_t script[0x100], code[0x400];
static int failed, passed;

#define LOCAL(i) (*(int32_t *)(script + 0x3C + 4 * (i)))
#define COND     (script[0xC5])

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (ok) passed++; else failed++;
}

static size_t put(size_t n, const void *p, size_t len) { memcpy(code + n, p, len); return n + len; }
static size_t put8(size_t n, uint8_t v) { code[n] = v; return n + 1; }
static size_t put_int(size_t n, int32_t v) { n = put8(n, 0x01); return put(n, &v, 4); }
static size_t put_local0(size_t n) { n = put8(n, 0x03); return put(n, "\0\0", 2); }
static size_t put_str(size_t n, const char *s)
{
    size_t len = strlen(s);
    n = put8(put8(n, 0x0E), (uint8_t)len);
    return put(n, s, len);
}

static void call_on(uint8_t *sc, uint8_t *at, int op)
{
    *(uint8_t **)(sc + 0x10) = *(uint8_t **)(sc + 0x14) = at;
    sc[0xD2] = 0;
    sc[0xC5] = 0xAA;
    (*(Handler **)0x469FEE)[op / 100](sc, op);
}

static void *got_obj;
static const char *got_params;
static uint8_t cmd_script[0x100], cmd_code[64];
static int32_t nested;

/* As MoonLoader's handler does: an opcode on the stub's own script, base
 * and IP swapped to a parameter buffer, while 0AA5 is still calling. */
static void __cdecl callback(void *obj, const char *params)
{
    static uint8_t buf[8] = { 0x03, 0x00, 0x00 };
    uint8_t *base = *(uint8_t **)(cmd_script + 0x10), *ip = *(uint8_t **)(cmd_script + 0x14);
    int32_t local0 = *(int32_t *)(cmd_script + 0x3C);
    got_obj = obj;
    got_params = params;
    *(int32_t *)(cmd_script + 0x3C) = 77;
    *(uint8_t **)(cmd_script + 0x10) = *(uint8_t **)(cmd_script + 0x14) = buf;
    (*(Handler **)0x469FEE)[0x0B4F / 100](cmd_script, 0x0B4F);
    nested = *(int32_t *)(cmd_script + 0x3C);
    *(uint8_t **)(cmd_script + 0x10) = base;
    *(uint8_t **)(cmd_script + 0x14) = ip;
    *(int32_t *)(cmd_script + 0x3C) = local0;
}

__declspec(dllexport) int run(void)
{
    HMODULE m;
    RunFn run_op;
    char log[8192] = "";
    FILE *f;
    size_t n;
    int cleo5;

    setvbuf(stdout, NULL, _IONBF, 0);
    m = LoadLibraryA("moonsamp.asi");
    check(m != NULL, "moonsamp.asi loads");
    if (!m)
        return 1;
    check(GetModuleHandleA("CLEO.asi") != NULL, "it loads CLEO.asi");
    f = fopen("moonsamp.log", "rb");
    if (f) {
        log[fread(log, 1, sizeof log - 1, f)] = 0;
        fclose(f);
    }
    cleo5 = strstr(log, "CLEO 5.") != NULL;
    check(strstr(log, "CLEO 4.") != NULL || cleo5, "the log names the CLEO version");
    check(strstr(log, "opcode table installed") == NULL, "moonsamp leaves CLEO's opcode table in place");
    check(*(void **)0x469FEE != (void *)0x8A6168, "0x469FEE points at CLEO's table");
    check(*(uint8_t *)0x561931 == 0xE9, "isSampfuncsLoaded marker");
    check(*(uint8_t *)0x4667DB == 0xE9, "isCleoLoaded marker, which CLEO 5 does not set");

    LOCAL(0) = 77;
    n = put_local0(0);
    call_on(script, code, 0x0B4F);
    check(LOCAL(0) == 0 && *(uint8_t **)(script + 0x14) == code + n, "0B4F runs through CLEO's table");

    run_op = (RunFn)(void *)GetProcAddress(m, "moonsamp_run");
    {
        int32_t in[1] = { (int32_t)(uintptr_t)"moonsamp" }, out[1] = { 0 };
        check(run_op && run_op(0x0D4C, "s", in, 1, out) == 0 && out[0] == 8, "moonsamp_run has the NewOpcodes");
    }

    /* MoonLoader's command script: the stub at base + 32 (as in host_test) */
    {
        size_t k = 32;
        void *obj = (void *)0x0BADF00D;
        memcpy(cmd_code + k, "\x35\x0b\x03\x00\x00\xa5\x0a\x01", 8); k += 8;
        *(uint32_t *)(cmd_code + k) = (uint32_t)(uintptr_t)callback; k += 4;
        memcpy(cmd_code + k, "\x04\x02\x04\x02\x03\x00\x00\x01", 8); k += 8;
        *(uint32_t *)(cmd_code + k) = (uint32_t)(uintptr_t)obj; k += 4;
        memcpy(cmd_code + k, "\x00\x43\x0b", 3);
        n = put_str(0, "lua");
        n = put_int(n, -32);
        memcpy(code + 0x300, code, n);
        call_on(cmd_script, code + 0x300, 0x0C63);
        check(cmd_script[0xC5] == 1, "0C63 registers console command \"lua\"");
        *(uint8_t **)(cmd_script + 0x10) = *(uint8_t **)(cmd_script + 0x14) = cmd_code;
        /* under CLEO 5 the command's script is inside an opcode, as when a
         * hook fires (CLEO 4 reads any bit there as "a CLEO script") */
        if (cleo5)
            cmd_script[0xDF] |= 0x02;
        n = put_str(0, "lua print(1)");
        got_obj = NULL;
        nested = -1;
        call_on(script, code, 0x0C62);
        check(got_obj == obj && got_params && strcmp(got_params, "print(1)") == 0,
              "0C62 runs the stub (under CLEO 5 with its script inside an opcode)");
        check(nested == 0, "the callback runs an opcode on the stub's own script, as MoonLoader does");
        check(!cleo5 || (cmd_script[0xDF] & 0x02) != 0, "the script is still inside its opcode after");
        got_obj = NULL;
        call_on(script, code, 0x0C62);
        check(got_obj == obj, "and runs it again");
    }

    printf("%d passed, %d failed (%s)\n", passed, failed, cleo5 ? "CLEO 5" : "CLEO 4");
    fflush(stdout);
    return failed ? 1 : 0;
}
