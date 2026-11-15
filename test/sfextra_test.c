/* moonsamp's extra SAMPFUNCS opcodes (src/sfextra.c) under Wine, through
 * moonsamp_run, with gta_sa.exe mapped (see run.sh). Line of sight, screen
 * coordinates and the cursor need the game running and are left to the
 * in-game test. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef int (__cdecl *RunFn)(int, const char *, const int32_t *, int, int32_t *);
typedef int8_t (__attribute__((thiscall)) *Handler)(void *, int);

static int failed, passed;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (ok) passed++; else failed++;
}

static RunFn run_op;
static int32_t in[24], out[16];
static int cond;

static int32_t fi(float f) { int32_t i; memcpy(&i, &f, 4); return i; }
static float fo(int k) { float f; memcpy(&f, &out[k], 4); return f; }
static int32_t ptr(const void *p) { return (int32_t)(uintptr_t)p; }
static const char *str(int k) { return (const char *)(uintptr_t)out[k]; }
static int close_to(float a, float b) { return fabsf(a - b) < 0.001f; }

static void op(int code, const char *types, int nout)
{
    memset(out, 0x55, sizeof out);
    cond = run_op(code, types, in, nout, out);
}

static float math(int code, float x)
{
    in[0] = fi(x);
    op(code, "fo", 0);
    return fo(0);
}

/* A script run through the opcode table, as the game does: the opcode
 * before the IP, so a stub can tell which one it is. */
static uint8_t script[0x100], code[256];
#define IP   (*(uint8_t **)(script + 0x14))
#define COND (script[0xC5])
static void dispatch(int opcode, size_t len)
{
    Handler *table = *(Handler **)0x469FEE;
    (void)len;
    *(uint8_t **)(script + 0x10) = code;
    code[0] = (uint8_t)opcode, code[1] = (uint8_t)(opcode >> 8);
    IP = code + 2;
    COND = 0xAA;
    table[opcode / 100](script, opcode);
}

__declspec(dllexport) int run(void)
{
    static char buf[256], buf2[256];
    static const char line[] = "Cross_Lynch has taken control of the Materials Factory";
    HMODULE m;
    setvbuf(stdout, NULL, _IONBF, 0);
    m = LoadLibraryA("moonsamp.asi");
    check(m != NULL, "moonsamp.asi loads");
    if (!m)
        return 1;
    run_op = (RunFn)(void *)GetProcAddress(m, "moonsamp_run");
    check(run_op != NULL, "it exports moonsamp_run");
    if (!run_op)
        return 1;

    /* strings: buffers by address, text inline (PointOverAlert.cs's 0C29) */
    lstrcpyA(buf, line);
    in[0] = ptr(buf), in[1] = ptr("taken CONTROL");
    op(0x0C29, "iso", 0);
    check(cond == 1 && out[0] == ptr(strstr(buf, "taken")), "0C29 stristr finds \"taken CONTROL\" in a buffer, ignoring case");
    op(0x0C18, "iso", 0);
    check(cond == 0 && out[0] == 0, "0C18 strstr is case-sensitive");
    in[1] = ptr("Materials");
    op(0x0C18, "iso", 0);
    check(cond == 1 && out[0] == ptr(strstr(buf, "Materials")), "0C18 strstr finds a match");
    in[0] = ptr("abc"), in[1] = ptr("abc");
    op(0x0C14, "ss", 0);
    check(cond == 1, "0C14 strcmp: equal");
    in[1] = ptr("ABC");
    op(0x0C14, "ss", 0);
    check(cond == 0, "0C14 strcmp: case matters");
    op(0x0C21, "ss", 0);
    check(cond == 1, "0C21 stricmp: equal ignoring case");
    in[0] = ptr("abcdef"), in[1] = ptr("abcxyz"), in[2] = 3;
    op(0x0C25, "ssi", 0);
    check(cond == 1, "0C25 strncmp: the first 3 match");

    in[0] = ptr(buf), in[1] = ptr("one");
    op(0x0C13, "is", 0);
    check(strcmp(buf, "one") == 0, "0C13 strcpy into a buffer");
    in[1] = ptr(" two");
    op(0x0C15, "is", 0);
    check(strcmp(buf, "one two") == 0, "0C15 strcat");
    in[1] = ptr(" three"), in[2] = 3;
    op(0x0C27, "isi", 0);
    check(strcmp(buf, "one two th") == 0, "0C27 strncat takes 3");
    memset(buf2, 'x', 8);
    in[0] = ptr(buf2), in[1] = ptr("ab"), in[2] = 4;
    op(0x0C24, "isi", 0);
    check(memcmp(buf2, "ab\0\0xxxx", 8) == 0, "0C24 strncpy pads to its size and no further");
    in[0] = ptr("hello");
    op(0x0C17, "so", 0);
    check(out[0] == 5, "0C17 strlen");

    in[0] = ptr("Mixed Case"), in[1] = ptr(buf);
    op(0x0C26, "si", 0);
    check(strcmp(buf, "MIXED CASE") == 0, "0C26 strupr into a buffer");
    op(0x0C28, "si", 0);
    check(strcmp(buf, "mixed case") == 0, "0C28 strlwr");
    in[0] = ptr("abc");
    op(0x0C2D, "si", 0);
    check(strcmp(buf, "cba") == 0, "0C2D strrev");

    lstrcpyA(buf, "a,,b;c");
    in[0] = ptr(buf), in[1] = ptr(",;");
    op(0x0C16, "iso", 0);
    check(cond == 1 && strcmp(str(0), "a") == 0, "0C16 strtok: first token");
    in[0] = 0;
    op(0x0C16, "iso", 0);
    check(cond == 1 && strcmp(str(0), "b") == 0, "0C16 strtok with 0: next token, delimiters run together");
    op(0x0C16, "iso", 0);
    check(cond == 1 && strcmp(str(0), "c") == 0, "0C16 strtok: last token");
    op(0x0C16, "iso", 0);
    check(cond == 0 && out[0] == 0, "0C16 strtok: then none");

    lstrcpyA(buf, "key=value=x");
    in[0] = ptr(buf), in[1] = '=';
    op(0x0C2A, "iio", 0);
    check(cond == 1 && out[0] == ptr(buf + 3), "0C2A strchr: the first '='");
    op(0x0C2C, "iio", 0);
    check(cond == 1 && out[0] == ptr(buf + 9), "0C2C strrchr: the last '='");
    in[1] = '#';
    op(0x0C2A, "iio", 0);
    check(cond == 0 && out[0] == 0, "0C2A strchr: none");
    in[1] = ptr("=x");
    op(0x0C2B, "iso", 0);
    check(cond == 1 && out[0] == ptr(buf + 3), "0C2B strpbrk");
    in[0] = ptr("abc123"), in[1] = ptr("0123456789");
    op(0x0C19, "sso", 0);
    check(out[0] == 3, "0C19 strcspn");
    in[0] = ptr("123abc");
    op(0x0C2E, "sso", 0);
    check(out[0] == 3, "0C2E strspn");

    in[0] = ptr(" -123");
    op(0x0C1A, "so", 0);
    check(out[0] == -123, "0C1A atoi");
    in[0] = ptr("-3.5e1");
    op(0x0C1B, "so", 0);
    check(close_to(fo(0), -35.0f), "0C1B atof");
    lstrcpyA(buf, "0x1Fz");
    in[0] = ptr(buf), in[1] = 0;
    op(0x0C2F, "iioo", 0);
    check(out[0] == ptr(buf + 4) && out[1] == 31, "0C2F strtol: radix 0 reads hex, the end at 'z'");
    in[0] = 255, in[1] = 16, in[2] = ptr(buf);
    op(0x0C1C, "iii", 0);
    check(strcmp(buf, "ff") == 0, "0C1C itoa radix 16");
    in[0] = -42, in[1] = 10;
    op(0x0C1C, "iii", 0);
    check(strcmp(buf, "-42") == 0, "0C1C itoa radix 10, negative");

    {
        static const uint8_t bytes[3] = { 0x01, 0xAB, 0xFF };
        uint8_t back[4] = { 0, 0, 0, 0x77 };
        in[0] = ptr(bytes), in[1] = ptr(buf), in[2] = 3;
        op(0x0C22, "iii", 0);
        check(cond == 1 && strcmp(buf, "01ABFF") == 0, "0C22 bin to hex");
        in[0] = ptr("01abff"), in[1] = ptr(back), in[2] = 3;
        op(0x0C23, "sii", 0);
        check(cond == 1 && memcmp(back, bytes, 3) == 0 && back[3] == 0x77, "0C23 hex to bin, within its size");
    }

    in[1] = ptr("some text");
    op(0x0C3A, "ose", 0);
    check(strcmp(str(0), "some text") == 0, "0C3A string pointer of inline text");

    /* memory and arrays */
    {
        int32_t arr[4] = { 10, 20, 30, 40 };
        uint8_t mem[8] = { 0 };
        in[0] = ptr(arr), in[1] = 2;
        op(0x0C0E, "iio", 0);
        check(cond == 1 && out[0] == 30, "0C0E array element 2");
        in[2] = 99;
        op(0x0C0F, "iii", 0);
        check(arr[2] == 99, "0C0F sets it");
        in[0] = ptr(arr), in[1] = 4, in[2] = 2;
        op(0x0C0C, "iiio", 0);
        check(out[0] == 20, "0C0C reads 2 bytes at offset 4");
        in[3] = 0x1234;
        op(0x0C0D, "iiii", 0);
        check(arr[1] == 0x1234, "0C0D writes 2 bytes at offset 4");
        in[0] = ptr(mem), in[1] = 0x5A, in[2] = 6;
        op(0x0C11, "iii", 0);
        check(mem[5] == 0x5A && mem[6] == 0, "0C11 memset");
        in[0] = ptr(mem + 2), in[1] = ptr(mem), in[2] = 6;
        op(0x0C10, "iii", 0);
        check(cond == 1 && mem[7] == 0x5A && mem[6] == 0x5A, "0C10 memcpy, overlapping");
        in[0] = ptr(mem), in[1] = ptr(mem + 1), in[2] = 4;
        op(0x0C12, "iii", 0);
        check(cond == 1, "0C12 memcmp: equal");
        in[0] = ptr(arr), in[1] = 3, in[2] = 4;
        op(0x0C1D, "iiio", 0);
        check(out[0] == 40, "0C1D array element with a size");
        in[3] = 7;
        op(0x0C1E, "iiii", 0);
        check(arr[3] == 7, "0C1E sets it");
        in[0] = ptr(buf), in[1] = 2, in[2] = 8, in[3] = ptr("third");
        op(0x0C20, "iiis", 0);
        check(strcmp(buf + 16, "third") == 0, "0C20 string array element 2 of 8 bytes");
        op(0x0C1F, "iiio", 0);
        check(out[0] == ptr(buf + 16), "0C1F its address");
    }

    /* math: radians */
    check(close_to(math(0x0C00, -2.5f), 2.5f), "0C00 abs");
    check(close_to(math(0x0C01, 3.14159265f), 180.0f), "0C01 radian to angle");
    check(close_to(math(0x0C02, 90.0f), 1.5707963f), "0C02 angle to radian");
    check(close_to(math(0x0C03, 1.5707963f), 1.0f), "0C03 sine");
    check(close_to(math(0x0C04, 1.0f), 1.5707963f), "0C04 arcsine");
    check(close_to(math(0x0C05, 3.14159265f), -1.0f), "0C05 cosine");
    check(close_to(math(0x0C06, 0.0f), 1.5707963f), "0C06 arccosine");
    check(close_to(math(0x0C07, 0.7853982f), 1.0f), "0C07 tangent");
    check(close_to(math(0x0C08, 1.0f), 0.7853982f), "0C08 arctangent");
    check(math(0x0C0A, 1.2f) == 2.0f && math(0x0C0A, -1.2f) == -1.0f, "0C0A ceil");
    check(math(0x0C0B, 1.8f) == 1.0f && math(0x0C0B, -1.2f) == -2.0f, "0C0B floor");
    in[0] = fi(2.0f), in[1] = fi(10.0f);
    op(0x0C09, "ffo", 0);
    check(close_to(fo(0), 1024.0f), "0C09 power 2^10");
    in[0] = fi(-2.0f), in[1] = fi(3.0f);
    op(0x0C09, "ffo", 0);
    check(close_to(fo(0), -8.0f), "0C09 power (-2)^3");
    in[0] = fi(9.0f), in[1] = fi(0.5f);
    op(0x0C09, "ffo", 0);
    check(close_to(fo(0), 3.0f), "0C09 power 9^0.5");

    /* quaternions through the game's own CQuaternion::Set / CMatrix::SetRotate */
    {
        int i;
        static const float rot[9] = { 0, 1, 0, -1, 0, 0, 0, 0, 1 };   /* 90 degrees about z */
        for (i = 0; i < 9; i++)
            in[i] = fi(rot[i]);
        op(0x0C32, "fffffffffoooo", 0);
        check(close_to(fo(0), 0) && close_to(fo(1), 0) && close_to(fo(2), 0.7071068f) && close_to(fo(3), 0.7071068f),
              "0C32 matrix to quat: 90 degrees about z");
        for (i = 0; i < 4; i++)
            in[i] = out[i];
        op(0x0C33, "ffffooooooooo", 0);
        for (i = 0; i < 9 && close_to(fo(i), rot[i]); i++)
            ;
        check(i == 9, "0C33 quat to matrix gives it back");
    }

    /* colours, tick count, modules, environment, unicode */
    in[0] = (int32_t)0x80FF4020;
    op(0x0B66, "ioooo", 0);
    check(out[0] == 0x80 && out[1] == 0xFF && out[2] == 0x40 && out[3] == 0x20, "0B66 argb to a, r, g, b");
    in[0] = 0x80, in[1] = 0xFF, in[2] = 0x40, in[3] = 0x20;
    op(0x0B67, "iiiio", 0);
    check(out[0] == (int32_t)0x80FF4020, "0B67 a, r, g, b to argb");
    op(0x0BFE, "o", 0);
    check(out[0] != 0x55555555 && (DWORD)out[0] - GetTickCount() + 1000 < 2000, "0BFE get_tick_count");
    in[0] = ptr("kernel32.dll");
    op(0x0C70, "so", 0);
    check(cond == 1 && out[0] == ptr(GetModuleHandleA("kernel32.dll")), "0C70 module handle");
    in[0] = out[0], in[1] = ptr("GetTickCount");
    op(0x0C71, "iso", 0);
    check(cond == 1 && out[0] == ptr((void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetTickCount")),
          "0C71 proc address");
    SetEnvironmentVariableA("MOONSAMP_TEST", "yes");
    in[0] = ptr("MOONSAMP_TEST"), in[1] = ptr(buf), in[2] = sizeof buf;
    op(0x0C67, "sii", 0);
    check(cond == 1 && strcmp(buf, "yes") == 0, "0C67 environment variable");
    {
        static WCHAR wide[16];
        in[0] = ptr("wide"), in[1] = ptr(wide), in[2] = 16;
        op(0x0C69, "sii", 0);
        check(cond == 1 && wide[0] == L'w' && wide[4] == 0, "0C69 ansi to unicode");
        in[0] = ptr(wide), in[1] = ptr(buf), in[2] = sizeof buf;
        op(0x0C68, "iii", 0);
        check(cond == 1 && strcmp(buf, "wide") == 0, "0C68 unicode to ansi");
    }

    /* a stub steps over its parameters (0C74 create_timer_interval: two
     * ints and the variable it would set), so the next opcode lines up */
    {
        size_t n = 2;
        code[n++] = 0x01; memset(code + n, 0, 4); n += 4;     /* int32 */
        code[n++] = 0x04; code[n++] = 0x10;                   /* int8 label */
        code[n++] = 0x03; code[n++] = 0x05; code[n++] = 0x00; /* @5 */
        code[n] = 0xEE;
        dispatch(0x0C74, n);
        check(IP == code + n && COND == 0, "a stubbed opcode (0C74) steps over its parameters and is false");
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed != 0;
}
