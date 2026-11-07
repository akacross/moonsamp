/* NewOpcodes (DK22Pac's CLEO plugin, 0D00-0D7F) without CLEO: matrices,
 * quaternions, bones, car components, collisions, strings and game state.
 * The drawing and CLEO audio stream opcodes are not here yet.
 *
 * MoonLoader cannot run an opcode it has no function for, so
 * moonloader/lib/newopcodes.lua runs these through moonsamp_run. As in
 * NewOpcodes, a matrix, quat or vector argument of 1 to 10 names one of ten
 * scratch ones, and a colpoint of 1 the scratch colpoint. */
#include "moonsamp.h"

#define ENTITY_RWOBJECT   0x18
#define ENTITY_MODEL      0x22      /* int16 */
#define ENTITY_TYPE       0x36      /* low 3 bits */
#define PLACEABLE_MATRIX  0x14
#define PHYS_NUM_COLLIDED 0xB9
#define PHYS_COLLIDED     0xBC      /* CEntity *[6] */
#define PED_MAX_HEALTH    0x544
#define VEH_CLASS         0x590
#define VEH_SUBCLASS      0x594
#define AUTO_DAMAGE       0x5A0     /* CAutomobile's CDamageManager */
#define MI_KEY            0x04
#define MI_TXD            0x0A      /* int16 */
#define VMI_STRUCT        0x5C      /* CVehicleStructure: CVector dummies[15] first */
#define FRAME_MODELLING   0x10
#define FRAME_CHILD       0x98
#define FRAME_NEXT        0x9C
#define ATOMIC_GEOMETRY   0x18
#define GEOM_MATERIALS    0x20
#define GEOM_NUM_MATS     0x24
#define BONE_OFFSET       0x04      /* AnimBlendFrameData */
#define BONE_KEYFRAME     0x10
#define BONE_SIZE         0x18
#define CLUMP_FRAMES      0x10      /* CAnimBlendClumpData */
#define CLUMP_NUM_FRAMES  0x08

#define MODEL_INFOS       (*(void ***)0x403DA7)     /* SA-MP R5 moves the array */
#define ANIM_CLUMP_OFFSET (*(int *)0xB5F878)
#define ATOMIC_VIS_OFFSET (*(int *)0x8D608C)
#define IGNORE_ENTITY     (*(void **)0xB7CD68)
#define CURRENT_COLOURS   ((uint8_t *)0xB7C4A0)
#define SUN_SCREEN_X      (*(float *)0xC3E028)
#define SUN_SCREEN_Y      (*(float *)0xC3E02C)
#define FAR_CLIP_Z        (*(float *)0xC3EF9C)
#define VECTOR_TO_SUN     ((float (*)[3])0xB7CA50)
#define STORED_VALUE      (*(uint32_t *)0xB79FD0)
#define OLD_WEATHER       (*(int16_t *)0xC81320)
#define MENU_MANAGER      ((uint8_t *)0xBA6748)     /* SFX volume at +0x4F, radio at +0x50 */
#define RS_GLOBAL         ((uint8_t *)0xC17040)     /* width at +4, height at +8 */

typedef void *(__cdecl *PoolGetFn)(int);
typedef void *(__cdecl *Mat3Fn)(void *, const void *, const void *);
typedef void *(__cdecl *RotateFn)(void *, const float *, float, int);
typedef float (__cdecl *AtanFn)(float, float);
typedef void (THISCALL *SetRotateFn)(void *, float, float, float);
typedef void (THISCALL *SetRotate1Fn)(void *, float);
typedef void (THISCALL *SetRotateQFn)(void *, const void *);
typedef void (THISCALL *QuatSetFn)(void *, const void *);
typedef void (THISCALL *QuatMulFn)(void *, const void *, const void *);
typedef void (THISCALL *QuatSlerpFn)(void *, const void *, const void *, float);
typedef void (THISCALL *EntityFn)(void *);
typedef void (THISCALL *AlphaFn)(void *, int);
typedef void *(__cdecl *PtrFn)(void *);
typedef int (__cdecl *IndexFn)(void *, int);
typedef void *(__cdecl *FindFn)(void *, const char *);
typedef void (THISCALL *VisibilityFn)(void *, void *, int);
typedef void (__cdecl *AtomicAlphaFn)(void *, int);
typedef void *(__cdecl *ObjectCb)(void *, void *);
typedef void *(__cdecl *ForAllFn)(void *, ObjectCb, void *);
typedef const char *(__cdecl *NameFn)(void *);
typedef void (THISCALL *WindowFn)(void *, uint8_t);
typedef int (__cdecl *LineOfSightFn)(const float *, const float *, void *, void **,
                                     int, int, int, int, int, int, int, int);
typedef int (__cdecl *TxdSlotFn)(const char *);
typedef uint8_t (__cdecl *LightColorFn)(void);
typedef uint8_t (THISCALL *GetLightFn)(void *, int);
typedef void (THISCALL *SetLightFn)(void *, int, int);
typedef float *(THISCALL *TransformFn)(void *, float *, const float *);
typedef int (__cdecl *ProjectileFn)(void *, int, float, float, float, float, const float *, void *);

#define GetPedAt                  ((PoolGetFn)0x54FF90)
#define GetVehicleAt              ((PoolGetFn)0x54FFF0)
#define GetObjectAt               ((PoolGetFn)0x550050)
#define RwMatrixMultiply        ((Mat3Fn)0x7F18B0)
#define RwMatrixRotate          ((RotateFn)0x7F1FD0)
#define RtQuatRotate            ((RotateFn)0x7EB7C0)
#define GetATanOfXY             ((AtanFn)0x53CC70)
#define CMatrix_SetRotate       ((SetRotateFn)0x59B120)
#define CMatrix_SetRotateXOnly  ((SetRotate1Fn)0x59AFA0)
#define CMatrix_SetRotateYOnly  ((SetRotate1Fn)0x59AFE0)
#define CMatrix_SetRotateZOnly  ((SetRotate1Fn)0x59B020)
#define CMatrix_SetRotateQuat   ((SetRotateQFn)0x59BBF0)
#define CQuat_SetMatrix         ((QuatSetFn)0x59C3E0)
#define CQuat_Multiply          ((QuatMulFn)0x59C270)
#define CQuat_Slerp             ((QuatSlerpFn)0x59C630)
#define UpdateRpHAnim           ((EntityFn)0x532B20)
#define SetRwObjectAlpha        ((AlphaFn)0x5332C0)
#define GetAnimHierarchy        ((PtrFn)0x734A40)
#define RpHAnimGetMatrixArray   ((PtrFn)0x7C5120)
#define RpHAnimIDGetIndex       ((IndexFn)0x7C51A0)
#define RpAnimBlendFindFrame    ((FindFn)0x4D62A0)
#define GetFrameFromName        ((FindFn)0x4C5400)
#define RwFrameGetLTM           ((PtrFn)0x7F0990)
#define RwFrameForAllObjects    ((ForAllFn)0x7F1200)
#define GetFrameNodeName        ((NameFn)0x72FB30)
#define SetComponentVisibility  ((VisibilityFn)0x6D2700)
#define SetComponentAtomicAlpha ((AtomicAlphaFn)0x6D2960)
#define SetWindowOpenFlag       ((WindowFn)0x6D3080)
#define ClearWindowOpenFlag     ((WindowFn)0x6D30B0)
#define ProcessLineOfSight      ((LineOfSightFn)0x56BA00)
#define FindTxdSlot             ((TxdSlotFn)0x731850)
#define LightsNorthSouth        ((LightColorFn)0x49D3A0)
#define LightsWestEast          ((LightColorFn)0x49D350)
#define GetLightStatus          ((GetLightFn)0x6C2130)
#define SetLightStatus          ((SetLightFn)0x6C2100)
#define TransformFromObject     ((TransformFn)0x533560)
#define AddProjectile           ((ProjectileFn)0x737C80)

static float vectors[10][3], quats[10][4];
static float matrices[10][16] __attribute__((aligned(16)));
static uint8_t colpoint[0x2C];

static float fsqrt(float x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

static float fabsf_(float x) { return x < 0 ? -x : x; }

/* A NULL one reads as zeroes and takes writes, so a missing matrix is not
 * a crash. */
static float sink[16] __attribute__((aligned(16)));

static void *scratch(Script *s, void *base, size_t size)
{
    uint32_t v = (uint32_t)script_int(s);
    if (!v) {
        memset(sink, 0, sizeof sink);
        return sink;
    }
    return v - 1 < 10 ? (uint8_t *)base + (v - 1) * size : (void *)(uintptr_t)v;
}

static float *matrix_arg(Script *s) { return scratch(s, matrices, sizeof matrices[0]); }
static float *quat_arg(Script *s)   { return scratch(s, quats, sizeof quats[0]); }
static float *vector_arg(Script *s) { return scratch(s, vectors, sizeof vectors[0]); }

static uint8_t *colpoint_arg(Script *s)
{
    uint32_t v = (uint32_t)script_int(s);
    if (!v) {
        memset(sink, 0, sizeof sink);
        return (uint8_t *)sink;
    }
    return v == 1 ? colpoint : (uint8_t *)(uintptr_t)v;
}

static void read_floats(Script *s, float *f, int n)
{
    int i;
    script_collect(s, n);
    for (i = 0; i < n; i++)
        f[i] = ScriptParams[i].f;
}

static void store_floats(Script *s, const float *f, int n)
{
    int i;
    for (i = 0; i < n; i++)
        ScriptParams[i].f = f[i];
    script_store(s, n);
}

static void store_int(Script *s, int32_t v)
{
    ScriptParams[0].i = v;
    script_store(s, 1);
}

static void store_float(Script *s, float v)
{
    ScriptParams[0].f = v;
    script_store(s, 1);
}

static void store_ptr(Script *s, const void *p) { store_int(s, (int32_t)(uintptr_t)p); }

/* A string parameter is inline text, not a number holding a char *. */
static int is_text(Script *s) { return *s->ip < 0x01 || *s->ip > 0x08; }

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return (uint8_t)*a - (uint8_t)*b;
}

static size_t slen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

static const char *sfind(const char *s, const char *sub)
{
    size_t n = slen(sub);
    for (; *s; s++)
        if (!memcmp(s, sub, n))
            return s;
    return n ? NULL : s;
}

static void *rwobject(void *entity) { return entity ? AT(entity, ENTITY_RWOBJECT, void *) : NULL; }

static void *model_info(int id)
{
    void **infos = MODEL_INFOS;
    int count = infos == (void **)0xA9B0C8 ? 20000 : 65535;
    return (unsigned)id < (unsigned)count ? infos[id] : NULL;
}

/* ---- matrices ------------------------------------------------------------ */

/* 0D00 matrix %3d% = matrix %1d% * matrix %2d% */
static int8_t __stdcall op_0D00(Script *s)
{
    float *a = matrix_arg(s), *b = matrix_arg(s), *out = matrix_arg(s);
    RwMatrixMultiply(out, a, b);
    return 0;
}

/* 0D01 rotate_matrix %1d% on_axis %2d% %3d% %4d% angle %5d% combine_op %6d% */
static int8_t __stdcall op_0D01(Script *s)
{
    float *m = matrix_arg(s), axis[4];
    read_floats(s, axis, 4);
    RwMatrixRotate(m, axis, axis[3], script_int(s));
    return 0;
}

/* Degrees from 0 to 360 of atan(x, y) - 90. */
static float angle_of(float x, float y)
{
    float a = GetATanOfXY(x, y) * 57.295776f - 90.0f;
    while (a < 0)
        a += 360.0f;
    return a;
}

static float elevation(const float *v) { return angle_of(v[2], fsqrt(v[0] * v[0] + v[1] * v[1])); }

/* 0D02 / 0D03 %2d% = matrix %1d% x_angle / y_angle: of its right / up vector */
static int8_t __stdcall op_0D02(Script *s) { store_float(s, elevation(matrix_arg(s))); return 0; }
static int8_t __stdcall op_0D03(Script *s) { store_float(s, elevation(matrix_arg(s) + 4)); return 0; }

/* 0D04 %2d% = matrix %1d% z_angle */
static int8_t __stdcall op_0D04(Script *s)
{
    float *m = matrix_arg(s);
    store_float(s, angle_of(m[0], m[1]));
    return 0;
}

/* 0D05 set_matrix %1d% position %2d% %3d% %4d% */
static int8_t __stdcall op_0D05(Script *s) { read_floats(s, matrix_arg(s) + 12, 3); return 0; }

/* 0D06 get_matrix %1d% position_to %2d% %3d% %4d% */
static int8_t __stdcall op_0D06(Script *s) { store_floats(s, matrix_arg(s) + 12, 3); return 0; }

/* a . (b x c) */
static float triple(const float *a, const float *b, const float *c)
{
    return a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
           a[2] * (b[0] * c[1] - b[1] * c[0]);
}

/* 0D07 get_coords %1d% %2d% %3d% offsets_relative_to_matrix %4d% store_to
 * %5d% %6d% %7d%: solves offset.x * right + offset.y * up + offset.z * at =
 * point - pos */
static int8_t __stdcall op_0D07(Script *s)
{
    float p[3], *m, d[3], r[3] = { 0, 0, 0 }, det;
    int i;
    read_floats(s, p, 3);
    m = matrix_arg(s);
    for (i = 0; i < 3; i++)
        d[i] = p[i] - m[12 + i];
    det = triple(m, m + 4, m + 8);
    if (det != 0) {
        r[0] = triple(d, m + 4, m + 8) / det;
        r[1] = triple(m, d, m + 8) / det;
        r[2] = triple(m, m + 4, d) / det;
    }
    store_floats(s, r, 3);
    return 0;
}

/* 0D08 set_matrix %1d% rotation %2d% %3d% %4d%: keeps the position */
static int8_t __stdcall op_0D08(Script *s)
{
    float *m = matrix_arg(s), r[3], pos[3];
    read_floats(s, r, 3);
    memcpy(pos, m + 12, sizeof pos);
    CMatrix_SetRotate(m, r[0], r[1], r[2]);
    memcpy(m + 12, pos, sizeof pos);
    return 0;
}

/* 0D09 copy_matrix %1d% to %2d%: the second argument is copied into the first */
static int8_t __stdcall op_0D09(Script *s)
{
    float *dst = matrix_arg(s), *src = matrix_arg(s);
    memcpy(dst, src, sizeof matrices[0]);
    return 0;
}

/* 0D0A store_coords_to %5d% %6d% %7d% from_matrix %1d% with_offsets %2d% %3d% %4d% */
static int8_t __stdcall op_0D0A(Script *s)
{
    float *m = matrix_arg(s), off[3], r[3];
    int i;
    read_floats(s, off, 3);
    for (i = 0; i < 3; i++)
        r[i] = off[0] * m[i] + off[1] * m[4 + i] + off[2] * m[8 + i] + m[12 + i];
    store_floats(s, r, 3);
    return 0;
}

/* 0D13 / 0D14 / 0D15 set_matrix %1d% x_angle / y_angle / z_angle %2d% */
static int8_t __stdcall op_0D13(Script *s) { float *m = matrix_arg(s); CMatrix_SetRotateXOnly(m, script_float(s) * 0.0174532925f); return 0; }
static int8_t __stdcall op_0D14(Script *s) { float *m = matrix_arg(s); CMatrix_SetRotateYOnly(m, script_float(s) * 0.0174532925f); return 0; }
static int8_t __stdcall op_0D15(Script *s) { float *m = matrix_arg(s); CMatrix_SetRotateZOnly(m, script_float(s) * 0.0174532925f); return 0; }

/* 0D16 set_matrix %1d% rotation_from_quat %2d% */
static int8_t __stdcall op_0D16(Script *s)
{
    float *m = matrix_arg(s), *q = quat_arg(s);
    CMatrix_SetRotateQuat(m, q);
    return 0;
}

/* 0D1D matrix_slerp %1d% from_matrix %2d% to_matrix %3d% t %4d% */
static int8_t __stdcall op_0D1D(Script *s)
{
    float *out = matrix_arg(s), *a = matrix_arg(s), *b = matrix_arg(s), qa[4], qb[4], q[4];
    float t = script_float(s);
    CQuat_SetMatrix(qa, a);
    CQuat_SetMatrix(qb, b);
    CQuat_Slerp(q, qa, qb, t);
    CMatrix_SetRotateQuat(out, q);
    return 0;
}

/* 0D25 set_matrix %1d% elements %2d% ... %17d%: as raw words, flags too */
static int8_t __stdcall op_0D25(Script *s)
{
    float *m = matrix_arg(s);
    int i;
    script_collect(s, 16);
    for (i = 0; i < 16; i++)
        memcpy(m + i, &ScriptParams[i], 4);
    return 0;
}

/* ---- quaternions and vectors -------------------------------------------- */

/* 0D17 convert_matrix %1d% to_quat %2d% */
static int8_t __stdcall op_0D17(Script *s)
{
    float *m = matrix_arg(s), *q = quat_arg(s);
    CQuat_SetMatrix(q, m);
    return 0;
}

/* 0D18 rotate_quat %1d% axis_vector %2d% %3d% %4d% angle %5d% combine_op %6d% */
static int8_t __stdcall op_0D18(Script *s)
{
    float *q = quat_arg(s), axis[4];
    read_floats(s, axis, 4);
    RtQuatRotate(q, axis, axis[3], script_int(s));
    return 0;
}

/* 0D19 get_normalised_quat %1d% to_quat %2d% */
static int8_t __stdcall op_0D19(Script *s)
{
    float *src = quat_arg(s), *dst = quat_arg(s), q[4], len;
    int i;
    memcpy(q, src, sizeof q);
    len = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    if (len == 0) {
        q[3] = 1.0f;
    } else {
        len = fsqrt(len);
        for (i = 0; i < 4; i++)
            q[i] /= len;
    }
    memcpy(dst, q, sizeof q);
    return 0;
}

/* 0D1A quat %3d% = quat %1d% * quat %2d% */
static int8_t __stdcall op_0D1A(Script *s)
{
    float *a = quat_arg(s), *b = quat_arg(s), *out = quat_arg(s);
    CQuat_Multiply(out, a, b);
    return 0;
}

/* 0D1E quat_slerp %1d% from_quat %2d% to_quat %3d% t %4d% */
static int8_t __stdcall op_0D1E(Script *s)
{
    float *out = quat_arg(s), *a = quat_arg(s), *b = quat_arg(s);
    CQuat_Slerp(out, a, b, script_float(s));
    return 0;
}

/* 0D24 set_quat %1d% elements %2d% %3d% %4d% %5d% */
static int8_t __stdcall op_0D24(Script *s) { read_floats(s, quat_arg(s), 4); return 0; }

/* 0D29 get_quat %1d% elements_to %2d% %3d% %4d% %5d% */
static int8_t __stdcall op_0D29(Script *s) { store_floats(s, quat_arg(s), 4); return 0; }

/* 0D1C normalize_vector %1d%: a zero vector becomes (1, 0, 0) */
static int8_t __stdcall op_0D1C(Script *s)
{
    float *v = vector_arg(s), len = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (len > 0) {
        len = fsqrt(len);
        v[0] /= len, v[1] /= len, v[2] /= len;
    } else {
        v[0] = 1.0f;
    }
    return 0;
}

/* 0D26 set_vector %1d% elements %2d% %3d% %4d% */
static int8_t __stdcall op_0D26(Script *s) { read_floats(s, vector_arg(s), 3); return 0; }

/* 0D28 get_vector %1d% elements_to %2d% %3d% %4d% */
static int8_t __stdcall op_0D28(Script *s) { store_floats(s, vector_arg(s), 3); return 0; }

/* ---- bones --------------------------------------------------------------- */

/* 0D0B get_actor %1d% bone %2d% matrix_to %3d%: the bone's world matrix,
 * the ped's own without a skeleton */
static int8_t __stdcall op_0D0B(Script *s)
{
    void *ped = GetPedAt(script_int(s)), *clump = rwobject(ped), *h, *m = NULL;
    int bone = script_int(s), i;
    if (clump) {
        UpdateRpHAnim(ped);
        h = GetAnimHierarchy(clump);
        if (!h) {
            m = AT(ped, PLACEABLE_MATRIX, void *);
        } else if ((i = RpHAnimIDGetIndex(h, bone)) >= 0) {
            m = (uint8_t *)RpHAnimGetMatrixArray(h) + i * 64;
        }
    }
    store_ptr(s, m);
    script_cond(s, m != NULL);
    return 0;
}

/* 0D30 %3d% = actor %1d% bone %2d%: a bone name, or an index into the
 * clump's frames */
static int8_t __stdcall op_0D30(Script *s)
{
    void *ped = GetPedAt(script_int(s)), *clump = rwobject(ped), *bone = NULL;
    if (is_text(s)) {
        char name[128];
        script_string(s, name, sizeof name);
        if (clump && name[0])
            bone = RpAnimBlendFindFrame(clump, name);
    } else {
        uint32_t i = (uint32_t)script_int(s);
        void *data = clump ? AT(clump, ANIM_CLUMP_OFFSET, void *) : NULL;
        if (data && i < AT(data, CLUMP_NUM_FRAMES, uint32_t))
            bone = AT(data, CLUMP_FRAMES, uint8_t *) + i * BONE_SIZE;
    }
    store_ptr(s, bone);
    script_cond(s, bone != NULL);
    return 0;
}

/* 0D31 %2d% = bone %1d% offset_vector */
static int8_t __stdcall op_0D31(Script *s)
{
    uint8_t *bone = (uint8_t *)(uintptr_t)script_int(s);
    store_ptr(s, bone ? bone + BONE_OFFSET : NULL);
    return 0;
}

/* 0D32 %2d% = bone %1d% quat: its key frame starts with the rotation */
static int8_t __stdcall op_0D32(Script *s)
{
    uint8_t *bone = (uint8_t *)(uintptr_t)script_int(s);
    store_ptr(s, bone ? AT(bone, BONE_KEYFRAME, void *) : NULL);
    return 0;
}

/* ---- car components -------------------------------------------------------- */

/* The vehicle and its frame named by the next two parameters. */
static void *component(Script *s, void **vehicle)
{
    char name[128];
    void *car = GetVehicleAt(script_int(s)), *clump = rwobject(car);
    script_string(s, name, sizeof name);
    if (vehicle)
        *vehicle = car;
    return clump && name[0] ? GetFrameFromName(clump, name) : NULL;
}

/* 0D0C get_car %1d% component %2s% matrix_to %3d% */
static int8_t __stdcall op_0D0C(Script *s)
{
    void *f = component(s, NULL);
    store_ptr(s, f ? RwFrameGetLTM(f) : NULL);
    script_cond(s, f != NULL);
    return 0;
}

/* 0D0D %3d% = get_car %1d% component %2s% */
static int8_t __stdcall op_0D0D(Script *s)
{
    void *f = component(s, NULL);
    store_ptr(s, f);
    script_cond(s, f != NULL);
    return 0;
}

/* 0D0E set_car %1d% component %2s% state %3d% */
static int8_t __stdcall op_0D0E(Script *s)
{
    void *car, *f = component(s, &car);
    int state = script_int(s);
    if (f)
        SetComponentVisibility(car, f, state);
    script_cond(s, f != NULL);
    return 0;
}

static void *__cdecl alpha_cb(void *object, void *alpha)
{
    if (*(uint8_t *)object == 1)
        SetComponentAtomicAlpha(object, (int)(intptr_t)alpha);
    return object;
}

/* 0D12 set_car %1d% component %2s% model_alpha %3d% */
static int8_t __stdcall op_0D12(Script *s)
{
    void *f = component(s, NULL);
    int alpha = script_int(s);
    if (f)
        RwFrameForAllObjects(f, alpha_cb, (void *)(intptr_t)alpha);
    script_cond(s, f != NULL);
    return 0;
}

static uint8_t *frame_arg(Script *s) { return (uint8_t *)(uintptr_t)script_int(s); }

/* 0D1F child, 0D20 next_component, 0D74 parent_component, 0D21 name,
 * 0D22 world_matrix, 0D23 modelling_matrix of component %1d% */
static int8_t __stdcall op_0D1F(Script *s) { uint8_t *f = frame_arg(s); store_ptr(s, f ? AT(f, FRAME_CHILD, void *) : NULL); return 0; }
static int8_t __stdcall op_0D20(Script *s) { uint8_t *f = frame_arg(s); store_ptr(s, f ? AT(f, FRAME_NEXT, void *) : NULL); return 0; }
static int8_t __stdcall op_0D74(Script *s) { uint8_t *f = frame_arg(s); store_ptr(s, f ? AT(f, 4, void *) : NULL); return 0; }
static int8_t __stdcall op_0D21(Script *s) { uint8_t *f = frame_arg(s); store_ptr(s, f ? GetFrameNodeName(f) : ""); return 0; }
static int8_t __stdcall op_0D22(Script *s) { uint8_t *f = frame_arg(s); store_ptr(s, f ? RwFrameGetLTM(f) : NULL); return 0; }
static int8_t __stdcall op_0D23(Script *s) { uint8_t *f = frame_arg(s); store_ptr(s, f ? f + FRAME_MODELLING : NULL); return 0; }

/* The frame's atomics: counts them, or stops at number `want`. */
typedef struct { int count, want; void *found; } AtomicSearch;

static void *__cdecl atomic_cb(void *object, void *data)
{
    AtomicSearch *a = data;
    if (*(uint8_t *)object != 1)
        return object;
    if (a->count++ == a->want) {
        a->found = object;
        return NULL;
    }
    return object;
}

/* 0D75 %2d% = component %1d% num_objects */
static int8_t __stdcall op_0D75(Script *s)
{
    AtomicSearch a = { 0, -1, NULL };
    uint8_t *f = frame_arg(s);
    if (f)
        RwFrameForAllObjects(f, atomic_cb, &a);
    store_int(s, a.count);
    return 0;
}

/* 0D76 %3d% = component %1d% object %2d% */
static int8_t __stdcall op_0D76(Script *s)
{
    uint8_t *f = frame_arg(s);
    AtomicSearch a = { 0, script_int(s), NULL };
    if (f && a.want >= 0)
        RwFrameForAllObjects(f, atomic_cb, &a);
    store_ptr(s, a.found);
    script_cond(s, a.found != NULL);
    return 0;
}

/* 0D77 object_atomic %1d% hide %2d%: clears or sets rpATOMICRENDER */
static int8_t __stdcall op_0D77(Script *s)
{
    uint8_t *atomic = frame_arg(s);
    int hide = script_int(s);
    if (atomic)
        atomic[2] = hide ? 0 : 4;
    return 0;
}

static uint16_t *visibility_flags(uint8_t *atomic) { return (uint16_t *)(atomic + ATOMIC_VIS_OFFSET + 2); }

/* 0D78 %3d% = get_object %1d% atomic_flag %2d% */
static int8_t __stdcall op_0D78(Script *s)
{
    uint8_t *atomic = frame_arg(s);
    int flag = script_int(s);
    store_int(s, atomic && (*visibility_flags(atomic) & flag) ? 1 : 0);
    return 0;
}

/* 0D79 set_object %1d% atomic_flag %2d% state %3d% */
static int8_t __stdcall op_0D79(Script *s)
{
    uint8_t *atomic = frame_arg(s);
    int flag = script_int(s), on = script_int(s);
    if (atomic) {
        if (on)
            *visibility_flags(atomic) |= (uint16_t)flag;
        else
            *visibility_flags(atomic) &= (uint16_t)~flag;
    }
    return 0;
}

/* 0D7A %2d% = get_object %1d% num_materials */
static int8_t __stdcall op_0D7A(Script *s)
{
    uint8_t *atomic = frame_arg(s), *g = atomic ? AT(atomic, ATOMIC_GEOMETRY, uint8_t *) : NULL;
    store_int(s, g ? AT(g, GEOM_NUM_MATS, int32_t) : 0);
    script_cond(s, g != NULL);
    return 0;
}

/* 0D7B %3d% = get_object %1d% material %2d% texture */
static int8_t __stdcall op_0D7B(Script *s)
{
    uint8_t *atomic = frame_arg(s), *g = atomic ? AT(atomic, ATOMIC_GEOMETRY, uint8_t *) : NULL;
    int i = script_int(s);
    void *tex = NULL;
    int ok = g && i >= 0 && i < AT(g, GEOM_NUM_MATS, int32_t);
    if (ok)
        tex = *AT(g, GEOM_MATERIALS, void ***)[i];
    store_ptr(s, tex);
    script_cond(s, ok);
    return 0;
}

/* ---- entities ------------------------------------------------------------ */

static int8_t set_alpha(Script *s, PoolGetFn get)
{
    void *e = get(script_int(s));
    int alpha = script_int(s);
    if (rwobject(e))
        SetRwObjectAlpha(e, alpha);
    script_cond(s, rwobject(e) != NULL);
    return 0;
}

/* 0D0F / 0D10 / 0D11 set_car / set_actor / set_object %1d% model_alpha %2d% */
static int8_t __stdcall op_0D0F(Script *s) { return set_alpha(s, GetVehicleAt); }
static int8_t __stdcall op_0D10(Script *s) { return set_alpha(s, GetPedAt); }
static int8_t __stdcall op_0D11(Script *s) { return set_alpha(s, GetObjectAt); }

/* 0D1B get_entity %1d% type_to %2d% class_to %3d%: the class is its vtable */
static int8_t __stdcall op_0D1B(Script *s)
{
    uint8_t *e = frame_arg(s);
    ScriptParams[0].i = e ? e[ENTITY_TYPE] & 7 : 0;
    ScriptParams[1].i = e ? AT(e, 0, int32_t) : 0;
    script_store(s, 2);
    return 0;
}

/* 0D2A / 0D2B / 0D2C %2d% = get_car / get_actor / get_object %1d%
 * number_of_collided_entites */
static int8_t num_collided(Script *s, PoolGetFn get)
{
    uint8_t *e = get(script_int(s));
    store_int(s, e ? e[PHYS_NUM_COLLIDED] : 0);
    return 0;
}
static int8_t __stdcall op_0D2A(Script *s) { return num_collided(s, GetVehicleAt); }
static int8_t __stdcall op_0D2B(Script *s) { return num_collided(s, GetPedAt); }
static int8_t __stdcall op_0D2C(Script *s) { return num_collided(s, GetObjectAt); }

/* 0D34 / 0D35 / 0D36 store_car / store_actor / store_object %1d%
 * collided_entities_to %2d%..%7d% */
static int8_t collided(Script *s, PoolGetFn get)
{
    uint8_t *e = get(script_int(s));
    int i;
    for (i = 0; i < 6; i++)
        ScriptParams[i].p = e ? AT(e, PHYS_COLLIDED + 4 * i, void *) : NULL;
    script_store(s, 6);
    return 0;
}
static int8_t __stdcall op_0D34(Script *s) { return collided(s, GetVehicleAt); }
static int8_t __stdcall op_0D35(Script *s) { return collided(s, GetPedAt); }
static int8_t __stdcall op_0D36(Script *s) { return collided(s, GetObjectAt); }

/* 0D39 %2d% = actor %1d% max_health */
static int8_t __stdcall op_0D39(Script *s)
{
    uint8_t *ped = GetPedAt(script_int(s));
    store_float(s, ped ? AT(ped, PED_MAX_HEALTH, float) : 0);
    return 0;
}

/* 0D47 %2d% = model %1d% txd_id, 0D48 %2d% = model %1d% crc32_key */
static int8_t __stdcall op_0D47(Script *s)
{
    uint8_t *mi = model_info(script_int(s));
    store_int(s, mi ? AT(mi, MI_TXD, int16_t) : -1);
    script_cond(s, mi != NULL);
    return 0;
}

static int8_t __stdcall op_0D48(Script *s)
{
    uint8_t *mi = model_info(script_int(s));
    store_int(s, mi ? AT(mi, MI_KEY, int32_t) : 0);
    script_cond(s, mi != NULL);
    return 0;
}

/* 0D43 %2d% = txd %1s% id */
static int8_t __stdcall op_0D43(Script *s)
{
    char name[64];
    store_int(s, FindTxdSlot(script_string(s, name, sizeof name)));
    return 0;
}

/* ---- vehicles -------------------------------------------------------------- */

/* 0D33 set_car %1d% door %2d% window_state %3d% */
static int8_t __stdcall op_0D33(Script *s)
{
    void *car = GetVehicleAt(script_int(s));
    int door = script_int(s), open = script_int(s);
    if (car)
        (open ? SetWindowOpenFlag : ClearWindowOpenFlag)(car, (uint8_t)door);
    return 0;
}

/* Only automobiles (and their kinds, trailers too) have a CDamageManager. */
static uint8_t *damage_manager(uint8_t *car)
{
    uint32_t c = car ? AT(car, VEH_CLASS, uint32_t) : 0xFF;
    return c <= 4 || c == 11 ? car + AUTO_DAMAGE : NULL;
}

/* 0D5C %3d% = get_car %1d% light %2d% damage_state */
static int8_t __stdcall op_0D5C(Script *s)
{
    uint8_t *dm = damage_manager(GetVehicleAt(script_int(s)));
    int light = script_int(s);
    store_int(s, dm ? GetLightStatus(dm, light) : 0);
    return 0;
}

/* 0D5D set_car %1d% light %2d% damage_state %3d% */
static int8_t __stdcall op_0D5D(Script *s)
{
    uint8_t *dm = damage_manager(GetVehicleAt(script_int(s)));
    int light = script_int(s), state = script_int(s);
    if (dm)
        SetLightStatus(dm, light, state);
    return 0;
}

/* 0D5E get_vehicle %1d% class_to %2d% subclass_to %3d% */
static int8_t __stdcall op_0D5E(Script *s)
{
    uint8_t *car = GetVehicleAt(script_int(s));
    ScriptParams[0].i = car ? AT(car, VEH_CLASS, int32_t) : 0;
    ScriptParams[1].i = car ? AT(car, VEH_SUBCLASS, int32_t) : 0;
    script_store(s, 2);
    return 0;
}

/* 0D5F get_vehicle %1d% dummy_element %2d% position %3d% to %5d% %6d% %7d%
 * %4d%: %3d% world coordinates, %4d% the mirrored side */
static int8_t __stdcall op_0D5F(Script *s)
{
    uint8_t *car = GetVehicleAt(script_int(s)), *mi, *vs;
    int dummy = script_int(s), world = script_int(s), mirror = script_int(s), ok = 0;
    float p[3] = { 0, 0, 0 }, w[3];
    if (car && (unsigned)dummy < 15 && (mi = model_info(AT(car, ENTITY_MODEL, int16_t))) &&
        (vs = AT(mi, VMI_STRUCT, uint8_t *))) {
        memcpy(p, vs + dummy * 12, sizeof p);
        if (mirror)
            p[0] = -p[0];
        if (world) {
            TransformFromObject(car, w, p);
            memcpy(p, w, sizeof p);
        }
        ok = 1;
    }
    store_floats(s, p, 3);
    script_cond(s, ok);
    return 0;
}

/* 0D60 create_projectile_type %1d% launched_from_entity %2d% origin %3d%
 * %4d% %5d% target %6d% %7d% %8d% target_entity %9d% force %10d% */
static int8_t __stdcall op_0D60(Script *s)
{
    int type = script_int(s);
    void *from = (void *)(uintptr_t)script_int(s), *target;
    float o[3], t[3], force;
    read_floats(s, o, 3);
    read_floats(s, t, 3);
    target = (void *)(uintptr_t)script_int(s);
    force = script_float(s);
    script_cond(s, AddProjectile(from, type, o[0], o[1], o[2], force, t, target) & 0xFF);
    return 0;
}

/* ---- collisions ------------------------------------------------------------- */

/* 0D3A get_collision_between_points %1d% %2d% %3d% and %4d% %5d% %6d% flags
 * %7d%..%14d% ignore_entity %15d% colpoint_data_to %16d% store_point_to
 * %17d% %18d% %19d% entity_to %20d% */
static int8_t __stdcall op_0D3A(Script *s)
{
    float from[3], to[3], out[3] = { 0, 0, 0 };
    int32_t f[8];
    void *ignore, *hit = NULL;
    uint8_t *cp;
    int i, ok;
    read_floats(s, from, 3);
    read_floats(s, to, 3);
    script_collect(s, 8);
    for (i = 0; i < 8; i++)
        f[i] = ScriptParams[i].i;
    ignore = (void *)(uintptr_t)script_int(s);
    cp = colpoint_arg(s);
    IGNORE_ENTITY = ignore;
    ok = ProcessLineOfSight(from, to, cp, &hit, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]) & 0xFF;
    IGNORE_ENTITY = NULL;
    if (ok)
        memcpy(out, cp, sizeof out);
    else
        hit = NULL;
    for (i = 0; i < 3; i++)
        ScriptParams[i].f = out[i];
    ScriptParams[3].p = hit;
    script_store(s, 4);
    script_cond(s, ok);
    return 0;
}

/* 0D3B normal_XYZ_to, 0D3C surface_to, 0D3D lighting_to (day | night << 4),
 * 0D3E depth_to of colpoint %1d% */
static int8_t __stdcall op_0D3B(Script *s) { store_floats(s, (float *)(colpoint_arg(s) + 0x10), 3); return 0; }
static int8_t __stdcall op_0D3C(Script *s) { store_int(s, colpoint_arg(s)[0x23]); return 0; }
static int8_t __stdcall op_0D3D(Script *s) { store_int(s, colpoint_arg(s)[0x25]); return 0; }
static int8_t __stdcall op_0D3E(Script *s) { store_float(s, AT(colpoint_arg(s), 0x28, float)); return 0; }

/* 0D3F find_intersection_between_circles_xyr1 %1d% %2d% %3d% and_xyr2 %4d%
 * %5d% %6d% store_point1_to %7d% %8d% point2_to %9d% %10d% */
static int8_t __stdcall op_0D3F(Script *s)
{
    float c[6], p[4] = { 0, 0, 0, 0 }, dx, dy, d, a, h, x, y, rx, ry;
    int ok;
    read_floats(s, c, 6);
    dx = c[3] - c[0];
    dy = c[4] - c[1];
    d = fsqrt(dx * dx + dy * dy);
    ok = d > 0 && d <= c[2] + c[5] && d >= fabsf_(c[2] - c[5]);
    if (ok) {
        a = (c[2] * c[2] - c[5] * c[5] + d * d) / (2.0f * d);
        x = c[0] + dx * a / d;
        y = c[1] + dy * a / d;
        h = c[2] * c[2] - a * a;
        h = h > 0 ? fsqrt(h) : 0;
        rx = -dy * (h / d);
        ry = dx * (h / d);
        p[0] = x + rx, p[1] = y + ry, p[2] = x - rx, p[3] = y - ry;
    }
    store_floats(s, p, 4);
    script_cond(s, ok);
    return 0;
}

/* ---- memory and strings --------------------------------------------------- */

/* 0D27 copy_memory_from %1d% to %2d% size %3d% */
static int8_t __stdcall op_0D27(Script *s)
{
    void *src, *dst;
    script_collect(s, 3);
    src = ScriptParams[0].p;
    dst = ScriptParams[1].p;
    if (src && dst && ScriptParams[2].i > 0)
        memcpy(dst, src, (size_t)ScriptParams[2].i);
    return 0;
}

/* 0D37 struct %1d% param %2d% = %3d%, 0D38 %3d% = struct %1d% param %2d%:
 * 4-byte elements */
static int8_t __stdcall op_0D37(Script *s)
{
    script_collect(s, 3);
    if (ScriptParams[0].p)
        ((int32_t *)ScriptParams[0].p)[ScriptParams[1].i] = ScriptParams[2].i;
    return 0;
}

static int8_t __stdcall op_0D38(Script *s)
{
    script_collect(s, 2);
    store_int(s, ScriptParams[0].p ? ((int32_t *)ScriptParams[0].p)[ScriptParams[1].i] : 0);
    return 0;
}

/* 0D4E %4d% = struct %1d% offset %2d% size %3d% */
static int8_t __stdcall op_0D4E(Script *s)
{
    int32_t v = 0;
    script_collect(s, 3);
    if (ScriptParams[0].p && ScriptParams[2].i > 0)
        memcpy(&v, (uint8_t *)ScriptParams[0].p + ScriptParams[1].i, ScriptParams[2].i > 4 ? 4 : (size_t)ScriptParams[2].i);
    store_int(s, v);
    return 0;
}

/* 0D4F struct %1d% offset %2d% size %3d% = %4d%: text is copied whole */
static int8_t __stdcall op_0D4F(Script *s)
{
    uint8_t *at;
    int32_t size, v;
    script_collect(s, 3);
    at = ScriptParams[0].p ? (uint8_t *)ScriptParams[0].p + ScriptParams[1].i : NULL;
    size = ScriptParams[2].i;
    if (is_text(s)) {
        char text[256];
        script_string(s, text, sizeof text);
        if (at)
            memcpy(at, text, slen(text) + 1);
        return 0;
    }
    v = script_int(s);
    if (at && size > 0)
        memcpy(at, &v, size > 4 ? 4 : (size_t)size);
    return 0;
}

/* 0D49 %3d% = compare_strings %1s% %2s%: true when equal */
static int8_t __stdcall op_0D49(Script *s)
{
    char a[256], b[256];
    const char *x = script_string(s, a, sizeof a), *y = script_string(s, b, sizeof b);
    int r = streq(x, y);
    store_int(s, r);
    script_cond(s, r == 0);
    return 0;
}

/* 0D4A concatenate_strings %1d% %2s%: onto the buffer %1d% points at */
static int8_t __stdcall op_0D4A(Script *s)
{
    char *dst = (char *)(uintptr_t)script_int(s), buf[256];
    const char *src = script_string(s, buf, sizeof buf);
    if (dst)
        memcpy(dst + slen(dst), src, slen(src) + 1);
    return 0;
}

/* 0D4B %3d% = locate_substring %1s% %2s%: a pointer into %1s% */
static int8_t __stdcall op_0D4B(Script *s)
{
    static char text[256];
    char sub[256];
    const char *str = script_string(s, text, sizeof text), *at = sfind(str, script_string(s, sub, sizeof sub));
    store_ptr(s, at);
    script_cond(s, at != NULL);
    return 0;
}

/* 0D4C %2d% = string %1s% length */
static int8_t __stdcall op_0D4C(Script *s)
{
    char buf[256];
    store_int(s, (int32_t)slen(script_string(s, buf, sizeof buf)));
    return 0;
}

/* 0D4D copy_string %1s% to %2d%: into the buffer %2d% points at */
static int8_t __stdcall op_0D4D(Script *s)
{
    char buf[256];
    const char *src = script_string(s, buf, sizeof buf);
    char *dst = (char *)(uintptr_t)script_int(s);
    if (dst)
        memcpy(dst, src, slen(src) + 1);
    return 0;
}

/* ---- game state ------------------------------------------------------------ */

/* 0D2D get_local_time_year_to %1d% month_to %2d% day_of_week_to %3d% day_to
 * %4d% hour_to %5d% minute_to %6d% second_to %7d% milliseconds_to %8d% */
static int8_t __stdcall op_0D2D(Script *s)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    ScriptParams[0].i = t.wYear;
    ScriptParams[1].i = t.wMonth;
    ScriptParams[2].i = t.wDayOfWeek;
    ScriptParams[3].i = t.wDay;
    ScriptParams[4].i = t.wHour;
    ScriptParams[5].i = t.wMinute;
    ScriptParams[6].i = t.wSecond;
    ScriptParams[7].i = t.wMilliseconds;
    script_store(s, 8);
    return 0;
}

/* 0D2E set_thread %1d% var %2d% to %3d%, 0D2F %3d% = get_thread %1d% var
 * %2d%: another script's local variables */
static int8_t __stdcall op_0D2E(Script *s)
{
    Script *t;
    script_collect(s, 3);
    t = ScriptParams[0].p;
    if (t && (unsigned)ScriptParams[1].i < 34)
        t->locals[ScriptParams[1].i] = ScriptParams[2].i;
    return 0;
}

static int8_t __stdcall op_0D2F(Script *s)
{
    Script *t;
    script_collect(s, 2);
    t = ScriptParams[0].p;
    store_int(s, t && (unsigned)ScriptParams[1].i < 34 ? t->locals[ScriptParams[1].i] : 0);
    return 0;
}

/* 0D55 get_sun_colors_core_to %1d% %2d% %3d% glow_to %4d% %5d% %6d% */
static int8_t __stdcall op_0D55(Script *s)
{
    int i;
    for (i = 0; i < 6; i++)
        ScriptParams[i].i = AT(CURRENT_COLOURS, 0x30 + 2 * i, uint16_t);
    script_store(s, 6);
    return 0;
}

/* 0D56 get_sun_screen_coords_XY_to %1d% %2d% */
static int8_t __stdcall op_0D56(Script *s)
{
    float p[2] = { SUN_SCREEN_X, SUN_SCREEN_Y };
    store_floats(s, p, 2);
    return 0;
}

/* 0D57 get_sun_position_to %1d% %2d% %3d%: false while below the horizon */
static int8_t __stdcall op_0D57(Script *s)
{
    float *v = VECTOR_TO_SUN[STORED_VALUE & 15], d = FAR_CLIP_Z * 2.0f, p[3];
    p[0] = v[0] * d, p[1] = v[1] * d, p[2] = v[2] * d;
    store_floats(s, p, 3);
    script_cond(s, v[2] > -0.1f);
    return 0;
}

/* 0D58 get_sun_size_core_to %1d% glow_to %2d% */
static int8_t __stdcall op_0D58(Script *s)
{
    float size = AT(CURRENT_COLOURS, 0x3C, float), p[2] = { size * 2.7335f, size * 6.0f };
    store_floats(s, p, 2);
    return 0;
}

/* 0D59 %1d% = current_weather */
static int8_t __stdcall op_0D59(Script *s) { store_int(s, (uint16_t)OLD_WEATHER); return 0; }

/* 0D5A get_trafficlights_type_NS_current_color_to %1d% type_WE_current_color_to %2d% */
static int8_t __stdcall op_0D5A(Script *s)
{
    ScriptParams[0].i = LightsNorthSouth();
    ScriptParams[1].i = LightsWestEast();
    script_store(s, 2);
    return 0;
}

/* 0D72 get_sfx_volume_to %2d% radio_volume_to %3d% type %1d%, 0D73
 * get_screen_width_to %2d% height_to %3d% type %1d%: type 1 for floats */
static int8_t store_pair(Script *s, int a, int b)
{
    if (script_int(s)) {
        ScriptParams[0].f = (float)a;
        ScriptParams[1].f = (float)b;
    } else {
        ScriptParams[0].i = a;
        ScriptParams[1].i = b;
    }
    script_store(s, 2);
    return 0;
}
static int8_t __stdcall op_0D72(Script *s) { return store_pair(s, MENU_MANAGER[0x4F], MENU_MANAGER[0x50]); }
static int8_t __stdcall op_0D73(Script *s) { return store_pair(s, AT(RS_GLOBAL, 4, int32_t), AT(RS_GLOBAL, 8, int32_t)); }

/* ---- the runner -------------------------------------------------------------- */

/* Runs an opcode for lib/newopcodes.lua: types holds a letter per input,
 * i an int, f a float, s a char * written inline (up to 255 bytes); nout
 * local variables follow them, and are copied into out. Returns the
 * condition result. */
__declspec(dllexport) int __cdecl moonsamp_run(int op, const char *types, const int32_t *in, int nout, int32_t *out)
{
    uint8_t thread[0x100] __attribute__((aligned(4))), code[2048], *p = code;
    Script *s = (Script *)thread;
    OpcodeFn fn = script_handler((uint16_t)op);
    int i, k;
    if (!fn || !types || nout < 0 || nout > 32)
        return 0;
    for (k = 0; types[k]; k++) {
        size_t n = types[k] == 's' && in[k] ? slen((const char *)(uintptr_t)in[k]) : 256;
        if (p - code > (int)sizeof code - 300)
            return 0;
        if (types[k] == 'f') {
            *p++ = 0x06;
        } else if (types[k] == 's' && n < 256) {
            *p++ = 0x0E;
            *p++ = (uint8_t)n;
            memcpy(p, (const void *)(uintptr_t)in[k], n);
            p += n;
            continue;
        } else {
            *p++ = 0x01;
        }
        memcpy(p, &in[k], 4);
        p += 4;
    }
    for (i = 0; i < nout; i++) {
        *p++ = 0x03;
        *p++ = (uint8_t)i;
        *p++ = 0;
    }
    memset(thread, 0, sizeof thread);
    memcpy(s->name, "moonsamp", 8);
    s->base_ip = s->ip = code;
    s->is_active = 1;
    fn(s);
    for (i = 0; i < nout; i++)
        out[i] = s->locals[i];
    return s->cond_result != 0;
}

void newopcodes_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0D00, op_0D00 }, { 0x0D01, op_0D01 }, { 0x0D02, op_0D02 }, { 0x0D03, op_0D03 },
        { 0x0D04, op_0D04 }, { 0x0D05, op_0D05 }, { 0x0D06, op_0D06 }, { 0x0D07, op_0D07 },
        { 0x0D08, op_0D08 }, { 0x0D09, op_0D09 }, { 0x0D0A, op_0D0A }, { 0x0D0B, op_0D0B },
        { 0x0D0C, op_0D0C }, { 0x0D0D, op_0D0D }, { 0x0D0E, op_0D0E }, { 0x0D0F, op_0D0F },
        { 0x0D10, op_0D10 }, { 0x0D11, op_0D11 }, { 0x0D12, op_0D12 }, { 0x0D13, op_0D13 },
        { 0x0D14, op_0D14 }, { 0x0D15, op_0D15 }, { 0x0D16, op_0D16 }, { 0x0D17, op_0D17 },
        { 0x0D18, op_0D18 }, { 0x0D19, op_0D19 }, { 0x0D1A, op_0D1A }, { 0x0D1B, op_0D1B },
        { 0x0D1C, op_0D1C }, { 0x0D1D, op_0D1D }, { 0x0D1E, op_0D1E }, { 0x0D1F, op_0D1F },
        { 0x0D20, op_0D20 }, { 0x0D21, op_0D21 }, { 0x0D22, op_0D22 }, { 0x0D23, op_0D23 },
        { 0x0D24, op_0D24 }, { 0x0D25, op_0D25 }, { 0x0D26, op_0D26 }, { 0x0D27, op_0D27 },
        { 0x0D28, op_0D28 }, { 0x0D29, op_0D29 }, { 0x0D2A, op_0D2A }, { 0x0D2B, op_0D2B },
        { 0x0D2C, op_0D2C }, { 0x0D2D, op_0D2D }, { 0x0D2E, op_0D2E }, { 0x0D2F, op_0D2F },
        { 0x0D30, op_0D30 }, { 0x0D31, op_0D31 }, { 0x0D32, op_0D32 }, { 0x0D33, op_0D33 },
        { 0x0D34, op_0D34 }, { 0x0D35, op_0D35 }, { 0x0D36, op_0D36 }, { 0x0D37, op_0D37 },
        { 0x0D38, op_0D38 }, { 0x0D39, op_0D39 }, { 0x0D3A, op_0D3A }, { 0x0D3B, op_0D3B },
        { 0x0D3C, op_0D3C }, { 0x0D3D, op_0D3D }, { 0x0D3E, op_0D3E }, { 0x0D3F, op_0D3F },
        { 0x0D43, op_0D43 }, { 0x0D47, op_0D47 }, { 0x0D48, op_0D48 }, { 0x0D49, op_0D49 },
        { 0x0D4A, op_0D4A }, { 0x0D4B, op_0D4B }, { 0x0D4C, op_0D4C }, { 0x0D4D, op_0D4D },
        { 0x0D4E, op_0D4E }, { 0x0D4F, op_0D4F }, { 0x0D55, op_0D55 }, { 0x0D56, op_0D56 },
        { 0x0D57, op_0D57 }, { 0x0D58, op_0D58 }, { 0x0D59, op_0D59 }, { 0x0D5A, op_0D5A },
        { 0x0D5C, op_0D5C }, { 0x0D5D, op_0D5D }, { 0x0D5E, op_0D5E }, { 0x0D5F, op_0D5F },
        { 0x0D60, op_0D60 }, { 0x0D72, op_0D72 }, { 0x0D73, op_0D73 }, { 0x0D74, op_0D74 },
        { 0x0D75, op_0D75 }, { 0x0D76, op_0D76 }, { 0x0D77, op_0D77 }, { 0x0D78, op_0D78 },
        { 0x0D79, op_0D79 }, { 0x0D7A, op_0D7A }, { 0x0D7B, op_0D7B },
    };
    size_t i;
    int refused = 0;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        refused += !script_try_register(ops[i].op, ops[i].fn);
    if (refused)
        ms_log("newopcodes: CLEO refused %d opcodes (NewOpcodes.cleo installed?); moonsamp_run still has them",
               refused);
}
