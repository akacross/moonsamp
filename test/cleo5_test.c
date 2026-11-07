/* moonsamp's CLEO 5 opcodes (src/cleo5.c) without CLEO, under Wine, through
 * moonsamp_run as moonloader/lib/cleo5.lua calls it (see run.sh). Keys need
 * the game's window and audio streams BASS, so those only show they answer
 * without them. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef int (__cdecl *RunFn)(int, const char *, const int32_t *, int, int32_t *);
typedef const char *(__attribute__((thiscall)) *TextGetFn)(void *, const char *);

static int failed, passed;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (ok) passed++; else failed++;
}

static RunFn run_op;
static int32_t in[40], out[40];
static int cond;

static int32_t fi(float f) { int32_t i; memcpy(&i, &f, 4); return i; }
static float fo(int k) { float f; memcpy(&f, &out[k], 4); return f; }
static int32_t ptr(const void *p) { return (int32_t)(uintptr_t)p; }
static const char *str(int k) { return (const char *)(uintptr_t)out[k]; }

static void op(int code, const char *types, int nout)
{
    memset(out, 0x55, sizeof out);
    cond = run_op(code, types, in, nout, out);
}

static const char *text_get(const char *key) { return ((TextGetFn)0x6A0050)((void *)0xC1B340, key); }

static int file_has(const char *path, const char *text)
{
    static char buf[1 << 16];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0;
    if (f)
        fclose(f);
    buf[n] = 0;
    return strstr(buf, text) != NULL;
}

__declspec(dllexport) int run(void)
{
    HMODULE m;
    char dir[MAX_PATH], path[MAX_PATH];
    setvbuf(stdout, NULL, _IONBF, 0);
    m = LoadLibraryA("moonsamp.asi");
    check(m != NULL, "moonsamp.asi loads");
    if (!m)
        return 1;
    run_op = (RunFn)(void *)GetProcAddress(m, "moonsamp_run");
    if (!run_op)
        return 1;
    GetModuleFileNameA(NULL, dir, sizeof dir);
    *strrchr(dir, '\\') = 0;

    /* ---- core ---- */
    op(0x0DD5, "", 1);
    check(out[0] == 9, "0DD5 get_platform is PLATFORM_WINDOWS");
    op(0x2000, "", 1);
    check(out[0] == 0, "2000 no CLEO function call: no arguments");
    in[0] = 1; in[1] = 5; in[2] = 6;
    op(0x2002, "iiie", 0);
    check(cond == 1, "2002 cleo_return_with sets the condition and skips its values");
    in[0] = 5;
    op(0x2003, "ie", 0);
    check(cond == 0, "2003 cleo_return_fail is false");

    /* ---- math ---- */
    in[0] = 0x10; in[1] = 4;
    op(0x2700, "ii", 0);
    check(cond, "2700 bit 4 of 0x10 is set");
    in[0] = 0; in[1] = 3;
    op(0x2701, "vi", 0);
    check(out[0] == 8, "2701 set_bit writes the variable back");
    in[0] = 0xFF; in[1] = 0;
    op(0x2702, "vi", 0);
    check(out[0] == 0xFE, "2702 clear_bit");
    in[0] = 0; in[1] = 31; in[2] = 1;
    op(0x2703, "vii", 0);
    check(out[0] == (int32_t)0x80000000, "2703 toggle_bit on bit 31");
    in[0] = ptr("x");
    op(0x2704, "s", 0);
    check(cond, "2704 text is truthy");
    in[0] = 0;
    op(0x2704, "i", 0);
    check(!cond, "2704 0 is not");
    {
        int seen[3] = { 0 }, i, ok = 1;
        for (i = 0; i < 60; i++) {
            in[0] = 10; in[1] = 20; in[2] = 30;
            op(0x2705, "iiioe", 0);
            ok &= out[0] == 10 || out[0] == 20 || out[0] == 30;
            seen[out[0] / 10 - 1] = 1;
        }
        check(ok && seen[0] && seen[1] && seen[2], "2705 pick_random_int picks each value, into the last argument");
        in[0] = ptr("red"); in[1] = ptr("blue");
        op(0x2707, "ssoe", 0);
        check(!strcmp(str(0), "red") || !strcmp(str(0), "blue"), "2707 pick_random_text");
    }
    in[0] = fi(100);
    op(0x2708, "f", 0);
    check(cond, "2708 random_chance 100% is true");
    in[0] = fi(0);
    op(0x2708, "f", 0);
    check(!cond, "2708 random_chance 0% is false");

    /* ---- text ---- */
    in[0] = ptr("");
    op(0x2600, "s", 0);
    check(cond, "2600 is_text_empty");
    in[0] = ptr("MoonSamp"); in[1] = ptr("moonsamp"); in[2] = 1;
    op(0x2601, "ssi", 0);
    check(cond, "2601 equal ignoring case");
    in[2] = 0;
    op(0x2601, "ssi", 0);
    check(!cond, "2601 not equal with case");
    in[0] = ptr("MoonSamp"); in[1] = ptr("NSA"); in[2] = 1;
    op(0x2602, "ssi", 0);
    check(cond, "2602 is_text_in_text");
    in[1] = ptr("moon");
    op(0x2603, "ssi", 0);
    check(cond, "2603 is_text_prefix");
    in[1] = ptr("SAMP");
    op(0x2604, "ssi", 0);
    check(cond, "2604 is_text_suffix");
    in[0] = ptr("abcde");
    op(0x2608, "s", 1);
    check(out[0] == 5, "2608 get_text_length");

    in[0] = ptr("MSTEST"); in[1] = ptr("%d-%s %.2f|%05d|%x|%-3s|"); in[2] = 42; in[3] = ptr("ok");
    in[4] = fi(3.14159f); in[5] = -12; in[6] = 255; in[7] = ptr("a");
    op(0x2609, "ssisfiise", 0);
    check(!strcmp(text_get("MSTEST"), "42-ok 3.14|-0012|ff|a  |"), "2609 a formatted label, served by CText::Get");
    check(!strcmp(text_get("mstest"), "42-ok 3.14|-0012|ff|a  |"), "labels ignore case");
    check(*text_get("") == 0, "an empty key is empty text");
    sprintf(path, "%s\\test.fxt", dir);
    {
        FILE *f = fopen(path, "wb");
        fputs("# comment\r\nMSFXT1 first text  \r\nMSFXT2 second // note\r\n\r\nMSNOVAL\r\n", f);
        fclose(f);
    }
    in[0] = ptr("root:\\test.fxt");
    op(0x2606, "s", 0);
    check(cond && !strcmp(text_get("MSFXT1"), "first text") && !strcmp(text_get("MSFXT2"), "second"),
          "2606 load_fxt: keys, trimmed text, comments");
    in[0] = ptr("test.fxt");
    op(0x2607, "s", 0);
    check(cond, "2607 unload_fxt");
    {
        uint16_t n0 = *(uint16_t *)0xA44B68;
        uint8_t *line = (uint8_t *)0xA913E8 + n0 * 0x44;
        in[0] = fi(100); in[1] = fi(200); in[2] = ptr("%s"); in[3] = ptr("on screen");
        op(0x2605, "ffsse", 0);
        check(*(uint16_t *)0xA44B68 == n0 + 1 && *(float *)(line + 0x2C) == 100 && *(float *)(line + 0x30) == 200 &&
              !strcmp(text_get((const char *)line + 0x34), "on screen"),
              "2605 display_text_formatted: the next intro text line, its label served");
        *(uint16_t *)0xA44B68 = n0;
    }

    /* ---- input ---- */
    in[0] = 'A';
    op(0x2086, "i", 1);
    check(cond && !strcmp(str(0), "A"), "2086 get_key_name of A");
    in[0] = ptr("FEC_SPC"); in[1] = ptr("%s"); in[2] = ptr("SPACE");
    op(0x2609, "ssse", 0);
    in[0] = VK_SPACE;
    op(0x2086, "i", 1);
    check(cond && !strcmp(str(0), "SPACE"), "2086 a key named by the game's text");
    in[0] = 0xE8;
    op(0x2086, "i", 1);
    check(!cond, "2086 a key with no name is false");
    {
        uint32_t *a = (uint32_t *)(0xB70198 + 0xB70);
        memset(a, 0, 8 * 4);
        a[0] = 1001; a[1] = 2;      /* F1, second */
        a[2] = 1; a[3] = 1;         /* left mouse button, first */
        a[4] = 4; a[5] = 3;         /* wheel up: left out */
        in[0] = 0; in[1] = 0;
        op(0x2085, "iio", 0);
        check(cond && out[0] == VK_LBUTTON, "2085 get_controller_key: the first key in set order");
        in[1] = 1;
        op(0x2085, "iio", 0);
        check(cond && out[0] == VK_F1, "2085 the second, an RsKeyCode as its virtual key");
        in[1] = 2;
        op(0x2085, "iio", 0);
        check(!cond && out[0] == 0, "2085 no third: false, the variable left alone");
    }
    in[0] = VK_F5;
    op(0x2080, "i", 0);
    check(!cond, "2080 no key is pressed without the game's window");
    in[0] = 'A'; in[1] = 'Z';
    op(0x2081, "iio", 0);
    check(!cond, "2081 none in range");

    /* ---- files ---- */
    {
        char buf[16] = "", want[MAX_PATH];
        int32_t h;
        in[0] = ptr("root:\\sub/file.txt");
        op(0x2303, "s", 1);
        sprintf(want, "%s\\sub\\file.txt", dir);
        check(!strcmp(str(0), want), "2303 root: is the game folder, slashes turned");
        in[0] = ptr(".\\x.lua");
        op(0x2303, "s", 1);
        sprintf(want, "%s\\moonloader\\x.lua", dir);
        check(!strcmp(str(0), want), "2303 . is the moonloader folder");
        in[0] = ptr("data.bin"); in[1] = ptr("wb");
        op(0x0A9A, "ss", 1);
        h = out[0];
        check(cond && h, "0A9A opens a file to write");
        in[0] = h; in[1] = 5; in[2] = ptr("hello");
        op(0x2302, "iii", 0);
        check(cond, "2302 writes a block");
        in[0] = h;
        op(0x2300, "i", 1);
        check(out[0] == 5, "2300 its position");
        op(0x0A9B, "i", 0);
        in[0] = ptr("data.bin"); in[1] = ptr("rb");
        op(0x0A9A, "ss", 1);
        h = out[0];
        in[0] = h;
        op(0x0A9C, "i", 1);
        check(out[0] == 5, "0A9C its size");
        in[0] = h; in[1] = 5; in[2] = ptr(buf);
        op(0x2301, "iii", 0);
        check(cond && !memcmp(buf, "hello", 5), "2301 reads it back");
        in[1] = 1;
        op(0x2301, "iii", 0);
        check(!cond, "2301 false past the end");
        in[0] = h;
        op(0x0A9B, "i", 0);
        op(0x2300, "i", 1);
        check(out[0] == -1, "a closed file is no file");
        in[0] = ptr("data.bin");
        op(0x2305, "s", 7);
        check(cond && out[0] >= 2024 && out[1] >= 1 && out[1] <= 12, "2305 get_file_write_time");
        in[0] = ptr("nope.bin");
        op(0x2305, "s", 7);
        check(!cond, "2305 false for a missing file");
        in[0] = -1; in[1] = 0;
        op(0x2304, "iio", 0);
        check(!cond, "2304 get_script_filename: false for a MoonLoader script");
    }

    /* ---- debug ---- */
    in[0] = ptr("%s %d"); in[1] = ptr("traced"); in[2] = 7;
    op(0x2101, "ssie", 0);
    in[0] = ptr("logs.txt"); in[1] = 0; in[2] = ptr("line %d"); in[3] = 1;
    op(0x2102, "sisie", 0);
    in[0] = ptr("logs.txt"); in[1] = 1; in[2] = ptr("line %d"); in[3] = 2;
    op(0x2102, "sisie", 0);
    check(file_has("logs.txt", "line 1\r\n") && file_has("logs.txt", " line 2\r\n"), "2102 log_to_file, with a timestamp");
    in[0] = 0; in[1] = ptr("bp");
    op(0x2100, "ise", 0);

    /* ---- memory and scripts ---- */
    {
        uint8_t a[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, b[8] = { 0 };
        static uint8_t s1[0x100], s2[0x100];
        in[0] = ptr(a); in[1] = ptr(a + 2); in[2] = 4;
        op(0x2400, "iii", 0);
        check(a[2] == 1 && a[5] == 4, "2400 copy_memory overlapping forward");
        in[0] = ptr(a); in[1] = 2; in[2] = 2;
        op(0x2401, "iii", 1);
        check(out[0] == 0x0201, "2401 read_memory_with_offset");
        in[0] = ptr(b); in[1] = 1; in[2] = 6; in[3] = 0x1AB;
        op(0x2402, "iiii", 0);
        check(b[0] == 0 && b[1] == 0xAB && b[6] == 0xAB && b[7] == 0, "2402 over 4 bytes fills with the low byte");
        in[0] = ptr(a); in[1] = ptr(a); in[2] = 8;
        op(0x2407, "iii", 0);
        check(cond, "2407 is_memory_equal");
        in[1] = ptr(b);
        op(0x2407, "iii", 0);
        check(!cond, "2407 different memory");
        *(uint8_t **)s1 = s2;
        *(void **)0xA8B42C = s1;
        in[0] = ptr(s2);
        op(0x2405, "i", 0);
        check(cond, "2405 is_script_running in the active list");
        op(0x2408, "i", 0);
        check(*(uint32_t *)(s2 + 0xCC) == 0xFFFFFFFF, "2408 terminate_script: it never wakes");
        op(0x2404, "", 1);
        check(out[0] == ptr(s1), "2404 the head of the active list");
        *(void **)0xA8B42C = NULL;
        op(0x2405, "i", 0);
        check(!cond, "2405 not once off the list");
    }

    /* ---- INI ---- */
    sprintf(path, "%s\\t.ini", dir);
    WritePrivateProfileStringA("a", "k1", "1", path);
    WritePrivateProfileStringA("a", "k2", "2", path);
    WritePrivateProfileStringA("b", "k", "3", path);
    in[0] = ptr("t.ini"); in[1] = ptr("a"); in[2] = ptr("k1");
    op(0x2801, "sss", 0);
    check(cond && GetPrivateProfileIntA("a", "k1", -1, path) == -1 && GetPrivateProfileIntA("a", "k2", -1, path) == 2,
          "2801 delete_key_from_ini_file");
    in[0] = ptr("t.ini"); in[1] = ptr("b");
    op(0x2800, "ss", 0);
    check(cond && GetPrivateProfileIntA("b", "k", -1, path) == -1, "2800 delete_section_from_ini_file");
    in[0] = ptr("none.ini");
    op(0x2800, "ss", 0);
    check(!cond, "2800 false for a missing file");

    /* ---- audio, without BASS ---- */
    in[0] = 0x12345;
    op(0x2500, "i", 0);
    check(!cond, "2500 without BASS: not playing");
    op(0x2501, "i", 1);
    check(fo(0) == 0, "2501 and no length");

    {
        static char log[1 << 16];
        FILE *f = fopen("moonsamp.log", "rb");
        size_t n = f ? fread(log, 1, sizeof log - 1, f) : 0;
        if (f)
            fclose(f);
        log[n] = 0;
        check(strstr(log, "trace: traced 7") != NULL, "2101 trace to moonsamp.log");
        check(strstr(log, "breakpoint bp") != NULL, "2100 breakpoint logged");
        check(strstr(log, "CText::Get hooked") != NULL, "the text hook is installed with the first label");
    }

    printf("%d passed, %d failed\n", passed, failed);
    fflush(stdout);
    return failed ? 1 : 0;
}
