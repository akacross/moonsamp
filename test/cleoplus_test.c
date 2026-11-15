/* moonsamp's CLEO+ (src/cleoplus.c) under Wine, through moonsamp_run as
 * lib/cleoplus.lua calls it, with gta_sa.exe mapped (see run.sh). Peds,
 * cars and objects are fake ones in fake pools; what calls into the game
 * (tasks, streaming, object creation, the camera's matrix) is left to the
 * in-game test. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef int (__cdecl *RunFn)(int, const char *, const int32_t *, int, int32_t *);

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

static void op(int code, const char *types, int nout)
{
    memset(out, 0x55, sizeof out);
    cond = run_op(code, types, in, nout, out);
}

#define AT(p, off, T) (*(T *)((uint8_t *)(p) + (off)))

/* Fake CTasks: the vtable's GetSubTask (+8) and GetTaskType (+0x10). */
typedef struct FakeTask { void **vt; struct FakeTask *sub; int id; void *target[4]; } FakeTask;
static void *__attribute__((thiscall)) task_sub(FakeTask *t) { return t->sub; }
static int __attribute__((thiscall)) task_type(FakeTask *t) { return t->id; }
static void *task_vt[8] = { 0, 0, (void *)task_sub, 0, (void *)task_type };

static uint8_t peds[2 * 0x7C4], cars[2 * 0xA18], objs[2 * 0x19C];
static uint8_t ped_flags[2] = { 0x80, 0x01 }, car_flags[2] = { 0x80, 0x02 }, obj_flags[2] = { 0x80, 0x03 };
static struct { void *objects; uint8_t *flags; int size, first_free; }
    ped_pool = { peds, ped_flags, 2, 0 }, car_pool = { cars, car_flags, 2, 0 }, obj_pool = { objs, obj_flags, 2, 0 };
#define PED   (peds + 0x7C4)
#define CAR   (cars + 0xA18)
#define OBJ   (objs + 0x19C)   /* the pool holds CCutsceneObject-sized slots */
#define PED_H (1 << 8 | 0x01)
#define CAR_H (1 << 8 | 0x02)
#define OBJ_H (1 << 8 | 0x03)

__declspec(dllexport) int run(void)
{
    HMODULE m;
    static uint8_t handling[0xE0], stats[0x40], matrix[0x48];
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

    /* ---- cars ---- */
    in[0] = CAR_H; in[1] = 1;
    op(0x0E61, "ii", 0);
    check(AT(CAR, 0x45C, uint16_t) == 0xFFFF, "0E61 alarm on: 0xFFFF");
    in[0] = CAR_H;
    op(0x0E00, "i", 1);
    check(out[0] == 1, "0E00 reads it back as 1");
    in[0] = CAR_H; in[1] = 2;
    op(0x0E61, "ii", 0);
    in[0] = CAR_H;
    op(0x0E00, "i", 1);
    check(AT(CAR, 0x45C, uint16_t) == 0xFFFE && out[0] == 2, "0E61 / 0E00 mode 2: 0xFFFE");
    in[0] = 0x7FFF;
    op(0x0E00, "i", 1);
    check(out[0] == -1, "0E00 of a bad handle is -1");

    in[0] = CAR_H;
    op(0x0E08, "i", 0);
    check(!cond, "0E08 not a mission car");
    in[0] = CAR_H;
    op(0x0E09, "i", 0);
    in[0] = CAR_H;
    op(0x0E08, "i", 0);
    check(cond && CAR[0x4A4] == 2, "0E09 marks it, 0E08 sees it");

    AT(CAR, 0x4C8, void *) = CAR;
    in[0] = CAR_H;
    op(0x0E59, "i", 1);
    check(cond && out[0] == CAR_H, "0E59 trailer handle");
    in[0] = CAR_H;
    op(0x0E5A, "i", 1);
    check(!cond && out[0] == -1, "0E5A no tractor: -1");

    AT(CAR, 0xD8, float) = 12.5f;
    AT(CAR, 0xEC, float) = 1; AT(CAR, 0xF0, float) = 2; AT(CAR, 0xF4, float) = 3;
    in[0] = CAR_H;
    op(0x0E65, "i", 1);
    check(cond && close_to(fo(0), 12.5f), "0E65 collision intensity");
    in[0] = CAR_H;
    op(0x0E66, "i", 3);
    check(close_to(fo(0), 1) && close_to(fo(1), 2) && close_to(fo(2), 3), "0E66 collision coordinates");

    CAR[0x41] = 0;
    in[0] = CAR_H;
    op(0x0E93, "i", 0);
    check(cond, "0E93 not on a surface: in the air");
    CAR[0x41] = 2;
    in[0] = CAR_H;
    op(0x0E93, "i", 0);
    check(!cond, "0E93 on a solid surface: not");

    CAR[0x42] = 0x04 | 0x80 | 0x20;
    in[0] = CAR_H;
    op(0x0EAD, "i", 5);
    check(out[0] == 1 && out[1] == 0 && out[2] == 1 && out[3] == 0 && out[4] == 1,
          "0EAD proofs: bullet, fire, explosion, collision, melee");

    AT(CAR, 0x14, void *) = matrix;
    in[0] = CAR_H; in[1] = fi(5); in[2] = fi(6); in[3] = fi(7);
    op(0x0EB4, "ifff", 0);
    check(close_to(AT(matrix, 0x30, float), 5) && close_to(AT(matrix, 0x38, float), 7), "0EB4 writes the matrix position");
    in[0] = ptr(CAR);
    op(0x0EEE, "i", 3);
    check(close_to(fo(0), 5) && close_to(fo(1), 6) && close_to(fo(2), 7), "0EEE coordinates by pointer");

    AT(CAR, 0x20, uint16_t) = 4321;
    in[0] = CAR_H;
    op(0x0EC9, "i", 1);
    check(out[0] == 4321, "0EC9 random seed");

    *(uint32_t *)0xB7CB7C = 10000;
    AT(CAR, 0x4D0, uint32_t) = 0;
    in[0] = CAR_H; in[1] = 500;
    op(0x0ECD, "ii", 0);
    check(AT(CAR, 0x4D0, uint32_t) == 10500, "0ECD don't delete until now + 500");
    in[0] = CAR_H; in[1] = 100;
    op(0x0ECD, "ii", 0);
    check(AT(CAR, 0x4D0, uint32_t) == 10500, "0ECD never shortens it");

    AT(CAR, 0x4C0, float) = 1000;
    in[0] = CAR_H;
    op(0x0ECF, "i", 1);
    check(out[0] == -1, "0ECF alive: -1");
    AT(CAR, 0x4C0, float) = 0;
    AT(CAR, 0x4D8, uint32_t) = 9000;
    in[0] = CAR_H;
    op(0x0ECF, "i", 1);
    check(out[0] == 1000, "0ECF dead for 1000 ms");

    in[0] = CAR_H; in[1] = 1;
    op(0x0EF6, "ib", 0);
    in[0] = CAR_H;
    op(0x0EF5, "i", 0);
    check(cond && (CAR[0x42A] & 2), "0EF6 / 0EF5 owned by the player");
    in[0] = CAR_H; in[1] = 0;
    op(0x0EF6, "ib", 0);
    in[0] = CAR_H;
    op(0x0EF5, "i", 0);
    check(!cond, "0EF6 clears it");

    AT(CAR, 0x384, void *) = handling;
    handling[0xDE] = 7;
    handling[0xCD] = 8;
    AT(handling, 0xD8, int32_t) = 35000;
    in[0] = CAR_H;
    op(0x0EF9, "i", 1);
    check(out[0] == 7, "0EF9 handling anim group");
    in[0] = CAR_H;
    op(0x0EFB, "i", 0);
    check(cond, "0EFB convertible flag");
    in[0] = CAR_H;
    op(0x0EFC, "i", 1);
    check(out[0] == 35000, "0EFC monetary value");

    AT(CAR, 0x49C, float) = 0.75f;
    AT(CAR, 0x4A0, float) = 0.25f;
    in[0] = CAR_H;
    op(0x0EFD, "i", 2);
    check(close_to(fo(0), 0.75f) && close_to(fo(1), 0.25f), "0EFD pedals");

    CAR[0xBA] = 9;
    AT(CAR, 0x12C, float) = 0.5f;
    in[0] = CAR_H;
    op(0x0E90, "i", 1);
    check(out[0] == 9, "0E90 contact surface");
    in[0] = CAR_H;
    op(0x0E91, "i", 1);
    check(close_to(fo(0), 0.5f), "0E91 contact brightness");

    AT(CAR, 0x648 + 4 * 3, void *) = matrix;
    in[0] = CAR_H; in[1] = 3;
    op(0x0E82, "ii", 0);
    check(cond, "0E82 has car node 3");
    in[0] = CAR_H; in[1] = 4;
    op(0x0E82, "ii", 0);
    check(!cond, "0E82 not node 4");

    /* ---- chars ---- */
    AT(PED, 0x598, int32_t) = 7;
    AT(PED, 0x540, float) = 50;
    AT(PED, 0x544, float) = 200;
    in[0] = PED_H;
    op(0x0E5C, "i", 1);
    check(close_to(fo(0), 25), "0E5C health percent of a ped");
    AT(PED, 0x598, int32_t) = 0;
    *(uint8_t *)(0xB7CD98 + 0x14F) = 100;
    in[0] = PED_H;
    op(0x0E5C, "i", 1);
    check(close_to(fo(0), 50), "0E5C a player's max health comes from CPlayerInfo");

    PED[0x718] = 2;
    AT(PED, 0x5A0 + 2 * 0x1C + 4, int32_t) = 1;
    AT(PED, 0x5A0 + 2 * 0x1C + 8, int32_t) = 17;
    in[0] = PED_H;
    op(0x0E8B, "i", 1);
    check(out[0] == 1, "0E8B active weapon state");
    in[0] = PED_H;
    op(0x0E8C, "i", 1);
    check(out[0] == 17, "0E8C active weapon clip");

    in[0] = PED_H;
    op(0x0EAA, "i", 0);
    in[0] = PED_H;
    op(0x0EAB, "i", 1);
    check(out[0] == 51, "0EAA arrests, 0EAB reads ped state 51");

    in[0] = PED_H;
    op(0x0ECE, "i", 1);
    check(out[0] == -1, "0ECE alive: -1");
    AT(PED, 0x530, int32_t) = 55;
    AT(PED, 0x750, uint32_t) = 7000;
    in[0] = PED_H;
    op(0x0ECE, "i", 1);
    check(out[0] == 3000, "0ECE dead for 3000 ms");

    AT(PED, 0x59C, void *) = stats;
    AT(stats, 0, int32_t) = 12;
    stats[0x24] = 60;
    in[0] = PED_H;
    op(0x0EB1, "i", 1);
    check(out[0] == 12, "0EB1 ped stat id");
    in[0] = PED_H;
    op(0x0EFA, "i", 1);
    check(out[0] == 60, "0EFA fear");

    /* in a car the ped's own flags read as in the air: its car answers */
    PED[0x41] = 0;
    AT(PED, 0x58C, void *) = CAR;
    CAR[0x41] = 2;
    PED[0x46D] |= 1;
    in[0] = PED_H;
    op(0x0E92, "i", 0);
    check(!cond, "0E92 in a car on the ground: not in the air");
    PED[0x46D] &= ~1;
    in[0] = PED_H;
    op(0x0E92, "i", 0);
    check(cond, "0E92 on foot with no ground flags: in the air");
    AT(PED, 0x58C, void *) = NULL;

    in[0] = PED_H;
    op(0x0EA4, "i", 0);
    check(!cond, "0EA4 not on fire");
    AT(PED, 0x730, void *) = stats;
    in[0] = PED_H;
    op(0x0EA4, "i", 0);
    check(cond, "0EA4 on fire");

    in[0] = PED_H;
    op(0x0EAF, "i", 0);
    check(cond, "0EAF no player data: the weapon is shown");
    {
        static uint8_t pdata[0x40];
        AT(PED, 0x480, void *) = pdata;
        in[0] = PED_H;
        op(0x0EAF, "i", 0);
        check(!cond, "0EAF render weapon off");
        pdata[0x35] = 0x10;
        in[0] = PED_H;
        op(0x0EAF, "i", 0);
        check(cond, "0EAF render weapon on");
    }

    in[0] = PED_H;
    op(0x0E0B, "i", 0);
    in[0] = PED_H;
    op(0x0E0A, "i", 0);
    check(cond && AT(PED, 0x484, uint32_t) == 2, "0E0B / 0E0A mission char");

    /* ---- objects ---- */
    in[0] = OBJ_H;
    op(0x0E0C, "i", 0);
    check(!cond, "0E0C not a mission object");
    in[0] = OBJ_H;
    op(0x0E0D, "i", 0);
    in[0] = OBJ_H;
    op(0x0E0C, "i", 0);
    check(cond && OBJ[0x13C] == 6, "0E0D marks it mission 2 (6), 0E0C sees it");

    /* ---- the world ---- */
    in[0] = 9;
    op(0x0E05, "i", 0);
    op(0x0E04, "", 1);
    check(out[0] == 9 && *(int16_t *)0xC8131C == 9, "0E05 / 0E04 next weather");
    in[0] = fi(0.4f);
    op(0x0E07, "f", 0);
    op(0x0E06, "", 1);
    check(close_to(fo(0), 0.4f), "0E07 / 0E06 rain");
    *(uint16_t *)0xC81318 = 0xFFFF;
    op(0x0EB0, "", 1);
    check(out[0] == -1, "0EB0 no forced weather: -1");
    *(uint16_t *)0xC81318 = 4;
    op(0x0EB0, "", 1);
    check(out[0] == 4, "0EB0 forced weather 4");

    *(int32_t *)0xA476AC = 0x200;
    *(uint8_t *)(0xA49960 + 0x200) = 0;
    op(0x0E1D, "", 0);
    check(!cond, "0E1D not on a mission");
    in[0] = 1;
    op(0x0F16, "b", 0);
    op(0x0E1D, "", 0);
    check(cond, "0F16 sets it, 0E1D sees it");
    {
        /* script space moved, as a limit adjuster's MAINsegmentSize does */
        static uint8_t moved[0x400];
        uint32_t native = *(uint32_t *)0x5D5380, at = (uint32_t)(uintptr_t)moved;
        DWORD old;
        VirtualProtect((void *)0x5D5380, 4, PAGE_EXECUTE_READWRITE, &old);
        *(uint32_t *)0x5D5380 = at;
        moved[0x200] = 0;
        op(0x0E1D, "", 0);
        check(!cond, "0E1D reads the moved script space, not 0xA49960");
        in[0] = 1;
        op(0x0F16, "b", 0);
        check(moved[0x200] == 1 && *(uint8_t *)(0xA49960 + 0x200) == 1, "0F16 writes the moved one, not 0xA49960");
        *(uint32_t *)0x5D5380 = native;
    }

    *(uint8_t *)(0x969130 + 5) = 1;
    in[0] = 5;
    op(0x0E5D, "i", 0);
    check(cond, "0E5D cheat 5 active");
    in[0] = 500;
    op(0x0E5D, "i", 0);
    check(!cond, "0E5D out of range: no");

    AT(0xC17040, 4, int32_t) = 1600;
    AT(0xC17040, 8, int32_t) = 900;
    in[0] = fi(100); in[1] = fi(100);
    op(0x0E0F, "ff", 2);
    check(close_to(fo(0), 75) && close_to(fo(1), 93.3333f), "0E0F aspect ratio at 1600x900");

    *(float *)(0xB6F028 + 0xBFC) = 128;
    op(0x0EC7, "", 1);
    check(cond && close_to(fo(0), 128), "0EC7 fade alpha");

    /* ---- the camera ---- */
    *(uint8_t *)(0xB6F028 + 0x59) = 1;
    op(0x0F12, "", 2);
    check(out[0] == 0xB6F028 && out[1] == 0xB6F028 + 0x174 + 0x238, "0F12 TheCamera and the active CCam");
    in[0] = fi(90); in[1] = fi(-10);
    op(0x0F15, "ff", 0);
    op(0x0F14, "", 2);
    check(close_to(fo(0), 90) && close_to(fo(1), -10), "0F15 / 0F14 rotation input round trip");
    *(int16_t *)(0xB6F028 + 0x174 + 0x238 + 0x0C) = 4;
    op(0x0E64, "", 1);
    check(out[0] == 4, "0E64 active camera mode");

    /* ---- pickups ---- */
    {
        uint8_t *start = *(uint8_t **)0x48ADC3, *p = start + 5 * *(uint8_t *)0x4590E1;
        AT(p, 0x10, int16_t) = 80; AT(p, 0x12, int16_t) = -160; AT(p, 0x14, int16_t) = 24;
        AT(p, 0x18, int16_t) = 1239;
        AT(p, 0x1A, uint16_t) = 3;
        p[0x1C] = 2;
        in[0] = fi(10); in[1] = fi(-20); in[2] = fi(3); in[3] = 1;
        op(0x0E33, "fffb", 1);
        check(cond && out[0] == (5 | 3 << 16), "0E33 the pickup at a coordinate");
        in[0] = 5 | 3 << 16;
        op(0x0E34, "i", 1);
        check(cond && out[0] == 1239, "0E34 its model");
        in[0] = 5 | 3 << 16;
        op(0x0E39, "i", 1);
        check(out[0] == 2, "0E39 its type");
        in[0] = 5 | 3 << 16;
        op(0x0E38, "i", 1);
        check(cond && out[0] == ptr(p), "0E38 its pointer");
        in[0] = 5 | 4 << 16;
        op(0x0E34, "i", 1);
        check(!cond && out[0] == -1, "0E34 a stale handle: -1");
    }

    /* ---- random numbers: MSVC rand() after srand(1) gives 41, 18467 ---- */
    in[0] = 1; in[1] = 0; in[2] = 100;
    op(0x0EBC, "iii", 1);
    check(out[0] == 41, "0EBC seed 1 in [0, 100): 41, as CLEO+");
    in[0] = 1; in[1] = 0; in[2] = 100;
    op(0x0EBC, "iii", 1);
    check(out[0] == 41, "0EBC the same seed gives the same number");
    in[0] = 1; in[1] = fi(0); in[2] = fi(1);
    op(0x0EBD, "iff", 1);
    check(close_to(fo(0), 41 / 32767.0f), "0EBD seed 1 in [0, 1)");

    /* ---- quats: NewOpcodes' scratch quat 1 ---- */
    in[0] = 1; in[1] = fi(0); in[2] = fi(1); in[3] = fi(0);
    op(0x0EB3, "ifff", 0);
    in[0] = 1;
    op(0x0D29, "i", 4);
    check(close_to(fo(0) * fo(0) + fo(1) * fo(1) + fo(2) * fo(2) + fo(3) * fo(3), 1), "0EB3 a unit quat, read with NewOpcodes' 0D29");

    /* ---- colpoints, by pointer ---- */
    {
        static float col[11] = { 1.5f, -2.5f, 3.5f };
        in[0] = ptr(col);
        op(0x0EE1, "i", 3);
        check(close_to(fo(0), 1.5f) && close_to(fo(1), -2.5f) && close_to(fo(2), 3.5f), "0EE1 colpoint coordinates by pointer");
    }

    /* ---- AI flags, from fake task trees ---- */
    {
        static uint8_t intel[0x40];
        static FakeTask gun = { task_vt, NULL, 1031 }, kill = { task_vt, NULL, 1000 }, anim = { task_vt, NULL, 401 };
        kill.target[1] = PED;      /* kill ped on foot: its target at +16 */
        PED[0x36] = 3;             /* entity type: a ped */
        gun.sub = &kill;
        AT(PED, 0x47C, void *) = intel;
        AT(intel, 4, void *) = &gun;              /* primary 0 */
        AT(intel, 4 + 5 * 4, void *) = &anim;     /* secondary 0 */
        in[0] = PED_H;
        op(0x0E46, "i", 0);
        check(cond, "0E46 using a gun");
        in[0] = PED_H;
        op(0x0E47, "i", 0);
        check(!cond, "0E47 not fighting");
        in[0] = PED_H; in[1] = 2;
        op(0x0E4B, "ii", 0);
        check(cond, "0E4B a named anim among the secondary tasks");
        in[0] = PED_H; in[1] = 1;
        op(0x0E4B, "ii", 0);
        check(!cond, "0E4B none among the primary ones");
        in[0] = PED_H;
        op(0x0E44, "i", 1);
        check(cond && out[0] == PED_H, "0E44 the kill task's target");
        in[0] = PED_H; in[1] = 1000;
        op(0x0E42, "ii", 0);
        check(cond, "0E42 finds a sub task");
        in[0] = PED_H; in[1] = 0;
        op(0x0E4C, "ii", 0);
        check(cond, "0E4C killing something is important");
        AT(PED, 0x47C, void *) = NULL;
    }

    /* ---- the camera lock patches the mouse reads, and puts them back ---- */
    {
        uint32_t before = *(uint32_t *)0x510C28;
        in[0] = 0;
        op(0x0E60, "b", 0);
        check(*(uint32_t *)0x510C28 != before && **(float **)0x510C28 == 0, "0E60 off: the reads point at zero");
        check(*(uint16_t *)0x50FB10 == 0x0DD8 && **(float **)0x50FB12 == 0, "the fmul at 0050FB10 keeps its opcode");
        in[0] = 1;
        op(0x0E60, "b", 0);
        check(*(uint32_t *)0x510C28 == before && *(uint32_t *)0x50FB12 == 0x85A5C4, "0E60 on: put back");
    }

    /* ---- save slot, first start, model names ---- */
    op(0x0E2C, "", 1);
    check(out[0] == -1, "0E2C before any game: -1");
    op(0x0E2D, "", 0);
    check(cond, "0E2D first start");
    in[0] = 400;
    op(0x0F17, "i", 1);
    check(out[0] != 0 && *(char *)(uintptr_t)out[0] == 0, "0F17 a name slot (empty: no IDE loaded here)");

    /* ---- damage: nothing taken ---- */
    in[0] = PED_H;
    op(0x0EB5, "i", 4);
    check(!cond && out[0] == 0, "0EB5 no damage this frame");
    check(*(uint8_t *)0x4B5AF9 == 0xE9 && *(uint8_t *)0x6D804F == 0xE9 && *(uint8_t *)0x4B4029 == 0xE9,
          "0EB5 hooked the damage code on first use");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed != 0;
}
