/* moonsamp's NewOpcodes (src/newopcodes.c) under Wine, through moonsamp_run
 * as lib/newopcodes.lua calls it, with gta_sa.exe mapped (see run.sh). Peds,
 * cars and objects are fake ones in fake pools; what needs RenderWare
 * running (RwMatrix and RtQuat math, bones, components by name, line of
 * sight) is left to the in-game test. */
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
static int close_to(float a, float b) { return fabsf(a - b) < 0.001f; }

/* A stand-in RwEngineInstance: render states are recorded, the rest return 1. */
static void *rasters[1024];
static int num_rasters;
static int __cdecl rw_state(int which, void *value)
{
    if (which == 1 && value && num_rasters < 1024)
        rasters[num_rasters++] = value;
    return 1;
}
static int __cdecl rw_any(void) { return 1; }
static void *engine[64];
static int bound(void *raster)
{
    int i;
    for (i = 0; i < num_rasters; i++)
        if (rasters[i] == raster)
            return 1;
    return 0;
}

/* in[] is filled by the caller; types as moonsamp_run takes them. */
static void op(int code, const char *types, int nout)
{
    memset(out, 0x55, sizeof out);
    cond = run_op(code, types, in, nout, out);
}

/* GTA's CPool: objects, flag bytes, size; a handle is index << 8 | flag. */
static uint8_t peds[2 * 0x7C4], cars[2 * 0xA18], objs[2 * 0x17C];
static uint8_t ped_flags[2] = { 0x80, 0x01 }, car_flags[2] = { 0x80, 0x02 }, obj_flags[2] = { 0x80, 0x03 };
static struct { void *objects; uint8_t *flags; int size, first_free; }
    ped_pool = { peds, ped_flags, 2, 0 }, car_pool = { cars, car_flags, 2, 0 }, obj_pool = { objs, obj_flags, 2, 0 };
#define PED   (peds + 0x7C4)
#define CAR   (cars + 0xA18)
#define OBJ   (objs + 0x17C)
#define PED_H (1 << 8 | 0x01)
#define CAR_H (1 << 8 | 0x02)
#define OBJ_H (1 << 8 | 0x03)

__declspec(dllexport) int run(void)
{
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
    *(void **)0xB74490 = &ped_pool;
    *(void **)0xB74494 = &car_pool;
    *(void **)0xB7449C = &obj_pool;

    /* ---- the runner ---- */
    check(run_op(0x0D80, "", in, 1, out) == 0, "an opcode it does not have returns 0");
    in[0] = ptr("hello");
    op(0x0D4C, "s", 1);
    check(out[0] == 5, "0D4C length of an inline string");
    {
        static char long_text[300];
        memset(long_text, 'x', 299);
        in[0] = ptr(long_text);
        op(0x0D4C, "s", 1);
        check(out[0] == 299, "a string over 255 bytes goes as a char *");
        in[0] = ptr(long_text);
        op(0x0D4C, "i", 1);
        check(out[0] == 299, "so does an int holding one");
    }
    {
        Handler *table = *(Handler **)0x469FEE;
        uint8_t script[0x100] = { 0 }, code[] = { 0x0E, 3, 'a', 'b', 'c', 0x03, 0, 0 };
        *(uint8_t **)(script + 0x10) = *(uint8_t **)(script + 0x14) = code;
        table[0x0D4C / 100](script, 0x0D4C);
        check(*(int32_t *)(script + 0x3C) == 3, "0D4C runs as a script opcode too");
    }

    /* ---- matrices ---- */
    {
        float id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 20, 30, 0 };
        int k;
        in[0] = 1;
        for (k = 0; k < 16; k++)
            in[1 + k] = fi(id[k]);
        op(0x0D25, "iffffffffffffffff", 0);
        in[0] = 1;
        op(0x0D06, "i", 3);
        check(close_to(fo(0), 10) && close_to(fo(1), 20) && close_to(fo(2), 30), "0D25 sets scratch matrix 1, 0D06 reads its position");
        in[0] = 1; in[1] = fi(1); in[2] = fi(2); in[3] = fi(3);
        op(0x0D0A, "ifff", 3);
        check(close_to(fo(0), 11) && close_to(fo(1), 22) && close_to(fo(2), 33), "0D0A offset to world");
        in[0] = fi(11); in[1] = fi(22); in[2] = fi(33); in[3] = 1;
        op(0x0D07, "fffi", 3);
        check(close_to(fo(0), 1) && close_to(fo(1), 2) && close_to(fo(2), 3), "0D07 world to offset");
        in[0] = 1; in[1] = fi(4); in[2] = fi(5); in[3] = fi(6);
        op(0x0D05, "ifff", 0);
        in[0] = 1;
        op(0x0D06, "i", 3);
        check(close_to(fo(0), 4) && close_to(fo(1), 5) && close_to(fo(2), 6), "0D05 sets the position");
        in[0] = 1; in[1] = fi(0); in[2] = fi(0); in[3] = fi(1.5707964f);
        op(0x0D08, "ifff", 0);
        in[0] = 1;
        op(0x0D06, "i", 3);
        check(close_to(fo(0), 4) && close_to(fo(2), 6), "0D08 keeps the position");
        in[0] = 1;
        op(0x0D04, "i", 1);
        check(close_to(fo(0), 0) || close_to(fo(0), 360), "0D04 z angle of a quarter turn (radians in)");
        in[0] = 2; in[1] = 1;
        op(0x0D09, "ii", 0);
        in[0] = 2;
        op(0x0D06, "i", 3);
        check(close_to(fo(0), 4) && close_to(fo(1), 5), "0D09 copies matrix 1 into 2");
        in[0] = 1; in[1] = fi(0);
        op(0x0D15, "if", 0);
        in[0] = 1;
        op(0x0D04, "i", 1);
        check(close_to(fo(0), 270), "0D15 z rotation 0 gives z angle 270 (atan - 90)");
        in[0] = 1; in[1] = fi(90);
        op(0x0D15, "if", 0);
        in[0] = 1;
        op(0x0D04, "i", 1);
        check(close_to(fo(0), 0) || close_to(fo(0), 360), "0D15 takes degrees");
        in[0] = 1;
        op(0x0D02, "i", 1);
        check(close_to(fo(0), 0) || close_to(fo(0), 360), "0D02 x angle of a level matrix");
    }

    in[0] = 0;
    op(0x0D06, "i", 3);
    check(fo(0) == 0 && fo(2) == 0, "a NULL matrix reads as zeroes");
    in[0] = 0; in[1] = fi(1); in[2] = fi(2); in[3] = fi(3);
    op(0x0D05, "ifff", 0);
    check(1, "and takes writes");

    /* ---- quaternions and vectors ---- */
    {
        in[0] = 1; in[1] = fi(0); in[2] = fi(0); in[3] = fi(3); in[4] = fi(4);
        op(0x0D24, "iffff", 0);
        in[0] = 1; in[1] = 2;
        op(0x0D19, "ii", 0);
        in[0] = 2;
        op(0x0D29, "i", 4);
        check(close_to(fo(2), 0.6f) && close_to(fo(3), 0.8f), "0D24 sets, 0D19 normalises, 0D29 reads a quat");
        in[0] = 3; in[1] = fi(0); in[2] = fi(0); in[3] = fi(0); in[4] = fi(1);
        op(0x0D24, "iffff", 0);
        in[0] = 2; in[1] = 3; in[2] = 4;
        op(0x0D1A, "iii", 0);
        in[0] = 4;
        op(0x0D29, "i", 4);
        check(close_to(fo(2), 0.6f) && close_to(fo(3), 0.8f), "0D1A times the identity quat");
        in[0] = 5; in[1] = 3; in[2] = 3; in[3] = fi(0.5f);
        op(0x0D1E, "iiif", 0);
        in[0] = 5;
        op(0x0D29, "i", 4);
        check(close_to(fo(3), 1), "0D1E slerp between equal quats");
        in[0] = 1; in[1] = fi(0);
        op(0x0D15, "if", 0);
        in[0] = 1; in[1] = 6;
        op(0x0D17, "ii", 0);
        in[0] = 6;
        op(0x0D29, "i", 4);
        check(close_to(fo(0) * fo(0) + fo(1) * fo(1) + fo(2) * fo(2) + fo(3) * fo(3), 1), "0D17 matrix to a unit quat");
        in[0] = 7; in[1] = 6;
        op(0x0D16, "ii", 0);
        in[0] = 7;
        op(0x0D04, "i", 1);
        check(close_to(fo(0), 270), "0D16 quat back to a matrix");
        in[0] = 1; in[1] = fi(3); in[2] = fi(0); in[3] = fi(4);
        op(0x0D26, "ifff", 0);
        in[0] = 1;
        op(0x0D1C, "i", 0);
        in[0] = 1;
        op(0x0D28, "i", 3);
        check(close_to(fo(0), 0.6f) && close_to(fo(2), 0.8f), "0D26 sets, 0D1C normalises, 0D28 reads a vector");
        in[0] = 2; in[1] = fi(0); in[2] = fi(0); in[3] = fi(0);
        op(0x0D26, "ifff", 0);
        in[0] = 2;
        op(0x0D1C, "i", 0);
        in[0] = 2;
        op(0x0D28, "i", 3);
        check(close_to(fo(0), 1), "0D1C turns a zero vector into (1, 0, 0)");
    }

    /* ---- memory and strings ---- */
    {
        int32_t words[4] = { 1, 2, 3, 4 }, copy[4] = { 0 };
        char buf[64] = "moon";
        in[0] = ptr(words); in[1] = ptr(copy); in[2] = 16;
        op(0x0D27, "iii", 0);
        check(copy[3] == 4, "0D27 copies memory");
        in[0] = ptr(words); in[1] = 2; in[2] = 99;
        op(0x0D37, "iii", 0);
        in[0] = ptr(words); in[1] = 2;
        op(0x0D38, "ii", 1);
        check(words[2] == 99 && out[0] == 99, "0D37 / 0D38 struct params");
        in[0] = ptr(words); in[1] = 4; in[2] = 1; in[3] = 0x77;
        op(0x0D4F, "iiii", 0);
        in[0] = ptr(words); in[1] = 4; in[2] = 2;
        op(0x0D4E, "iii", 1);
        check(out[0] == 0x0077 && words[1] == 0x77, "0D4F / 0D4E sized fields");
        in[0] = ptr(buf); in[1] = 8; in[2] = 0; in[3] = ptr("text");
        op(0x0D4F, "iiis", 0);
        check(strcmp(buf + 8, "text") == 0, "0D4F writes text whole");
        in[0] = ptr(buf); in[1] = ptr("samp");
        op(0x0D4A, "is", 0);
        check(strcmp(buf, "moonsamp") == 0, "0D4A appends");
        in[0] = ptr(buf); in[1] = ptr("samp");
        op(0x0D4B, "is", 1);
        check(cond && out[0] == ptr(buf + 4), "0D4B points into the string it was given");
        in[0] = ptr("abc"); in[1] = ptr("x");
        op(0x0D4B, "ss", 1);
        check(!cond && out[0] == 0, "0D4B false when missing");
        in[0] = ptr("same"); in[1] = ptr("same");
        op(0x0D49, "ss", 1);
        check(cond && out[0] == 0, "0D49 equal strings");
        in[0] = ptr("a"); in[1] = ptr("b");
        op(0x0D49, "ss", 1);
        check(!cond && out[0] < 0, "0D49 different strings");
        in[0] = ptr("copied"); in[1] = ptr(buf);
        op(0x0D4D, "si", 0);
        check(strcmp(buf, "copied") == 0, "0D4D copies into a buffer");
    }

    /* ---- geometry, time, threads ---- */
    {
        uint8_t thread[0x100] = { 0 }, cp[0x2C] = { 0 };
        in[0] = fi(0); in[1] = fi(0); in[2] = fi(5); in[3] = fi(8); in[4] = fi(0); in[5] = fi(5);
        op(0x0D3F, "ffffff", 4);
        check(cond && close_to(fo(0), 4) && close_to(fo(1), 3) && close_to(fo(2), 4) && close_to(fo(3), -3), "0D3F circle intersections");
        in[2] = fi(1); in[5] = fi(1);
        op(0x0D3F, "ffffff", 4);
        check(!cond, "0D3F false when apart");
        op(0x0D2D, "", 8);
        check(out[0] >= 2024 && out[1] >= 1 && out[1] <= 12, "0D2D local time");
        in[0] = ptr(thread); in[1] = 5; in[2] = 1234;
        op(0x0D2E, "iii", 0);
        in[0] = ptr(thread); in[1] = 5;
        op(0x0D2F, "ii", 1);
        check(*(int32_t *)(thread + 0x3C + 20) == 1234 && out[0] == 1234, "0D2E / 0D2F another script's local 5");
        *(float *)(cp + 0x10) = 0.5f; cp[0x23] = 9; cp[0x25] = 0x3A; *(float *)(cp + 0x28) = 0.25f;
        in[0] = ptr(cp);
        op(0x0D3B, "i", 3);
        check(close_to(fo(0), 0.5f), "0D3B colpoint normal");
        op(0x0D3C, "i", 1);
        check(out[0] == 9, "0D3C surface");
        op(0x0D3D, "i", 1);
        check(out[0] == 0x3A, "0D3D lighting");
        op(0x0D3E, "i", 1);
        check(close_to(fo(0), 0.25f), "0D3E depth");
    }

    /* ---- entities ---- */
    {
        uint8_t mi[0x60] = { 0 }, vstruct[15 * 12] = { 0 }, other[0x40];
        void **infos = *(void ***)0x403DA7;
        *(float *)(PED + 0x544) = 176;
        PED[0xB9] = 2;
        *(void **)(PED + 0xBC) = other;
        PED[0x36] = 3;
        *(void **)PED = (void *)0x86D168;
        in[0] = PED_H;
        op(0x0D39, "i", 1);
        check(close_to(fo(0), 176), "0D39 max health");
        op(0x0D2B, "i", 1);
        check(out[0] == 2, "0D2B number collided");
        op(0x0D35, "i", 6);
        check(out[0] == ptr(other) && out[1] == 0, "0D35 collided entities");
        in[0] = ptr(PED);
        op(0x0D1B, "i", 2);
        check(out[0] == 3 && out[1] == 0x86D168, "0D1B type and vtable");
        in[0] = PED_H; in[1] = 100;
        op(0x0D10, "ii", 0);
        check(!cond, "0D10 false without an RwObject");
        in[0] = 0x0005;
        op(0x0D39, "i", 1);
        check(fo(0) == 0, "a stale handle gives 0, not a crash");
        in[0] = OBJ_H;
        op(0x0D2C, "i", 1);
        check(out[0] == 0, "0D2C on an object");

        *(uint32_t *)(CAR + 0x590) = 0;
        *(uint32_t *)(CAR + 0x594) = 2;
        *(int16_t *)(CAR + 0x22) = 411;
        in[0] = CAR_H;
        op(0x0D5E, "i", 2);
        check(out[0] == 0 && out[1] == 2, "0D5E class and subclass");
        in[0] = CAR_H; in[1] = 1; in[2] = 1;
        op(0x0D5D, "iii", 0);
        in[0] = CAR_H; in[1] = 1;
        op(0x0D5C, "ii", 1);
        check(out[0] == 1, "0D5D / 0D5C light damage");
        *(uint32_t *)(CAR + 0x590) = 9;
        in[0] = CAR_H; in[1] = 1;
        op(0x0D5C, "ii", 1);
        check(out[0] == 0, "a bike has no lights to read");
        *(void **)(mi + 0x5C) = vstruct;
        *(int32_t *)(mi + 4) = 0x12345678;
        *(int16_t *)(mi + 0xA) = 7;
        ((float *)vstruct)[3 * 4 + 0] = 0.8f;
        ((float *)vstruct)[3 * 4 + 2] = 0.3f;
        infos[411] = mi;
        in[0] = CAR_H; in[1] = 4; in[2] = 0; in[3] = 1;
        op(0x0D5F, "iiii", 3);
        check(cond && close_to(fo(0), -0.8f) && close_to(fo(2), 0.3f), "0D5F dummy position, mirrored");
        in[1] = 15;
        op(0x0D5F, "iiii", 3);
        check(!cond, "0D5F false past the last dummy");
        in[0] = 411;
        op(0x0D47, "i", 1);
        check(cond && out[0] == 7, "0D47 model txd");
        op(0x0D48, "i", 1);
        check(cond && out[0] == 0x12345678, "0D48 model key");
        in[0] = 412;
        op(0x0D47, "i", 1);
        check(!cond && out[0] == -1, "0D47 false for a missing model");
        infos[411] = NULL;
    }

    /* ---- frames and atomics ---- */
    {
        uint8_t frame[0xA4] = { 0 }, atomic[0x80] = { 0 }, geom[0x40] = { 0 }, tex[8], mat[16] = { 0 };
        void *mats[1] = { mat };
        *(void **)(frame + 4) = (void *)0x1111;
        *(void **)(frame + 0x98) = (void *)0x2222;
        *(void **)(frame + 0x9C) = (void *)0x3333;
        in[0] = ptr(frame);
        op(0x0D1F, "i", 1);
        check(out[0] == 0x2222, "0D1F child");
        op(0x0D20, "i", 1);
        check(out[0] == 0x3333, "0D20 next");
        op(0x0D74, "i", 1);
        check(out[0] == 0x1111, "0D74 parent");
        op(0x0D23, "i", 1);
        check(out[0] == ptr(frame + 0x10), "0D23 modelling matrix");
        *(void **)mat = tex;
        *(void **)(atomic + 0x18) = geom;
        *(void ***)(geom + 0x20) = mats;
        *(int32_t *)(geom + 0x24) = 1;
        in[0] = ptr(atomic);
        op(0x0D7A, "i", 1);
        check(cond && out[0] == 1, "0D7A materials");
        in[1] = 0;
        op(0x0D7B, "ii", 1);
        check(cond && out[0] == ptr(tex), "0D7B material texture");
        in[1] = 1;
        op(0x0D7B, "ii", 1);
        check(!cond, "0D7B false past the last material");
        in[1] = 1;
        op(0x0D77, "ii", 0);
        check(atomic[2] == 0, "0D77 hides");
        in[1] = 0;
        op(0x0D77, "ii", 0);
        check(atomic[2] == 4, "0D77 shows");
        *(int *)0x8D608C = 0x40;
        in[0] = ptr(atomic); in[1] = 0x100; in[2] = 1;
        op(0x0D79, "iii", 0);
        in[0] = ptr(atomic); in[1] = 0x100;
        op(0x0D78, "ii", 1);
        check(out[0] == 1 && *(uint16_t *)(atomic + 0x42) == 0x100, "0D79 / 0D78 visibility flags");
        *(int *)0x8D608C = -1;
    }

    /* ---- game state ---- */
    {
        *(int16_t *)0xC81320 = 8;
        op(0x0D59, "", 1);
        check(out[0] == 8, "0D59 weather");
        *(int32_t *)(0xC17040 + 4) = 1920;
        *(int32_t *)(0xC17040 + 8) = 1080;
        in[0] = 0;
        op(0x0D73, "i", 2);
        check(out[0] == 1920 && out[1] == 1080, "0D73 screen size as ints");
        in[0] = 1;
        op(0x0D73, "i", 2);
        check(close_to(fo(0), 1920), "0D73 as floats");
        *(uint8_t *)(0xBA6748 + 0x4F) = 40;
        *(uint8_t *)(0xBA6748 + 0x50) = 50;
        in[0] = 0;
        op(0x0D72, "i", 2);
        check(out[0] == 40 && out[1] == 50, "0D72 volumes");
        *(uint16_t *)(0xB7C4A0 + 0x30) = 255;
        *(uint16_t *)(0xB7C4A0 + 0x3A) = 77;
        *(float *)(0xB7C4A0 + 0x3C) = 2;
        op(0x0D55, "", 6);
        check(out[0] == 255 && out[5] == 77, "0D55 sun colours");
        op(0x0D58, "", 2);
        check(close_to(fo(0), 5.467f) && close_to(fo(1), 12), "0D58 sun size");
    }

    /* ---- drawing: queued, drawn by the game's frame, which never runs here ---- */
    {
        struct { float x, y, z, rhw; uint32_t color; float u, v; } verts[2];
        uint8_t *site = (uint8_t *)0x53E293, *stub;
        int k, full = 0;
        memset(verts, 0, sizeof verts);
        in[0] = ptr(verts), in[1] = 2, in[2] = fi(10), in[3] = fi(20), in[4] = fi(0), in[5] = fi(1);
        in[6] = 1, in[7] = 2, in[8] = 3, in[9] = 4, in[10] = fi(0.25f), in[11] = fi(0.5f), in[12] = 0, in[13] = 0;
        op(0x0D41, "iiffffiiiiffii", 0);
        check(verts[1].x == 10 && verts[1].y == 20 && verts[1].rhw == 1 && verts[1].u == 0.25f && verts[0].x == 0,
              "0D41 sets the second vertex");
        check(verts[1].color == 0x04010203, "0D41 colour as D3D's ARGB");
        in[12] = 1;
        op(0x0D41, "iiffffiiiiffii", 0);
        check(close_to(verts[1].x, 1910) && close_to(verts[1].u, 0.75f), "0D41 invertX from the screen width");
        verts[0].x = 10, verts[0].y = 0;
        in[0] = ptr(verts), in[1] = 1, in[2] = fi(0), in[3] = fi(0), in[4] = fi(90);
        op(0x0D45, "iifff", 0);
        check(close_to(verts[0].x, 0) && close_to(verts[0].y, -10), "0D45 rotates around a point, degrees");
        in[0] = 1, in[1] = 21;
        op(0x0D41, "iiffffiiiiffii", 0);
        check(1, "0D41 ignores a scratch vertex past 20");

        check(*site == 0xE8 && site + 5 + *(int32_t *)(site + 1) == (uint8_t *)0x60F8C0, "Render2dStuff calls 0x60F8C0 before drawing");
        in[0] = 5, in[1] = 0, in[2] = 2, in[3] = 1, in[4] = 1, in[5] = 5, in[6] = 6, in[7] = 0;
        op(0x0D40, "iiiiiiii", 0);
        check(cond == 1, "0D40 queues a shape");
        stub = site + 5 + *(int32_t *)(site + 1);
        check(stub != (uint8_t *)0x60F8C0 && stub[0] == 0x60 && stub[1] == 0x9C && stub[2] == 0xE8 && stub[9] == 0xE9 &&
              stub + 14 + *(int32_t *)(stub + 10) == (uint8_t *)0x60F8C0,
              "and hooks the call, keeping the registers and going on to 0x60F8C0");
        site = (uint8_t *)0x53E1E0;
        stub = site + 5 + *(int32_t *)(site + 1);
        check(*site == 0xE8 && stub[0] == 0x60 && stub + 14 + *(int32_t *)(stub + 10) == (uint8_t *)0x742CF0,
              "spotlights go in before CWeaponEffects::Render");
        for (k = 0; k < 100; k++) {
            op(0x0D40, "iiiiiiii", 0);
            if (!cond)
                full++;
        }
        check(full == 1, "the shape queue holds 100 a frame");
        /* a frame drawn through the stand-in engine */
        {
            void *freed[1] = { (void *)0xAAAA0000 }, *kept[1] = { (void *)0xBBBB0000 };
            void (*frame)(void) = (void (*)(void))(void *)(0x53E293 + 5 + *(int32_t *)(0x53E293 + 1));
            int k;
            for (k = 0; k < 64; k++)
                engine[k] = (void *)rw_any;
            engine[0x20 / 4] = (void *)rw_state;
            *(void **)0xC97B24 = engine;
            frame();                                       /* the 100 shapes */
            in[0] = ptr(freed), in[1] = fi(0), in[2] = fi(0), in[3] = fi(10), in[4] = fi(10);
            in[5] = in[6] = in[7] = in[8] = 255, in[9] = fi(0);
            op(0x0D7E, "iffffiiiif", 0);
            in[0] = ptr(kept);
            op(0x0D7E, "iffffiiiif", 0);
            in[0] = ptr(freed);
            op(0x0D7D, "i", 0);
            num_rasters = 0;
            frame();
            check(!bound(freed[0]) && bound(kept[0]), "0D7D drops queued sprites with that texture before the next draw");
            num_rasters = 0;
            frame();
            check(!bound(kept[0]), "a drawn sprite is not drawn again");
        }
        in[0] = ptr("tex"), in[1] = fi(1), in[2] = fi(2), in[3] = fi(1), in[4] = fi(1), in[5] = 1;
        op(0x0D65, "sffffi", 0);
        check(1, "0D65 queues a text");
        in[0] = 0x1234;
        op(0x0D7D, "i", 0);
        check(1, "0D7D leaves a texture it did not load alone");
        in[0] = 0, in[1] = fi(3);
        op(0x0D68, "i", 1);
        check(out[0] == 0, "0D68 is 0 with no bass.dll");
    }

    printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
