/* CLEO+ (JuniorDjjr's CLEO plugin, 0E00-0F17) without CLEO: the opcodes
 * MoonLoader, Lua, NewOpcodes and CLEO 5 do not already have.
 *
 * moonloader/lib/cleoplus.lua runs these through moonsamp_run (script.c).
 * Offsets are plugin-sdk's (c03810d, the one CLEO+ builds against). */
#include "moonsamp.h"

/* CEntity / CPhysical */
#define ENT_MATRIX           0x14
#define ENT_RWOBJECT         0x18
#define ENT_RANDOM_SEED      0x20
#define ENT_MODEL            0x22      /* int16 */
#define PLACEABLE_POS        0x04
#define PHYS_FLAGS1          0x41      /* 1 submerged, 2 on solid surface */
#define PHYS_FLAGS2          0x42      /* 4 bullet, 8 fire, 0x10 collision, 0x20 melee, 0x80 explosion proof */
#define PHYS_CONTACT_SURFACE 0xBA
#define PHYS_DAMAGE          0xD8
#define PHYS_LAST_COLLISION  0xEC
#define PHYS_BRIGHTNESS      0x12C
/* CVehicle */
#define VEH_HANDLING         0x384
#define VEH_OWNED_FLAGS      0x42A     /* 2 has been owned by the player */
#define VEH_ALARM            0x45C     /* uint16 */
#define VEH_GAS              0x49C
#define VEH_BRAKE            0x4A0
#define VEH_CREATED_BY       0x4A4     /* 2 mission vehicle */
#define VEH_HEALTH           0x4C0
#define VEH_TRACTOR          0x4C4
#define VEH_TRAILER          0x4C8
#define VEH_NEED_UNTIL       0x4D0
#define VEH_BLOWN_UP_AT      0x4D8
#define VEH_HORN_COUNTER     0x514
#define VEH_VT_PLAY_HORN     0xD8
#define AUTO_CAR_NODES       0x648
#define HAND_MODEL_FLAGS2    0xCD      /* 8 convertible */
#define HAND_VALUE           0xD8
#define HAND_ANIM_GROUP      0xDE
/* CPed */
#define PED_INTELLIGENCE     0x47C
#define PED_PLAYER_DATA      0x480
#define PED_CREATED_BY       0x484
#define PED_STATE            0x530
#define PED_MOVE_STATE       0x534
#define PED_HEALTH           0x540
#define PED_MAX_HEALTH       0x544
#define PED_NEED_UNTIL       0x54C
#define PED_VEHICLE          0x58C
#define PED_FLAGS_IN_VEHICLE 0x46D     /* m_nPedFlags bit 8: in a vehicle */
#define PED_TYPE             0x598
#define PED_STATS            0x59C
#define PED_WEAPONS          0x5A0     /* CWeapon[13], 0x1C each: type, state, clip */
#define PED_ACTIVE_SLOT      0x718
#define PED_FIRE             0x730
#define PED_DEATH_TIME       0x750
#define PPED_TARGETTED       0x79C     /* CPlayerPed */
#define PDATA_FLAGS          0x35      /* 0x10 render weapon */
#define PEDSTATE_ARRESTED    51
/* CObject */
#define OBJ_TYPE             0x13C     /* 2 mission, 6 mission 2 */
#define OBJ_MISSION          2
#define OBJ_MISSION2         6
/* CWeaponInfo */
#define WI_FIRE              0x00
#define WI_MODEL1            0x0C
#define WI_MODEL2            0x10
#define WI_SLOT              0x14
#define WI_FLAGS             0x18
#define WI_ANIM_GROUP        0x1C
#define WI_CLIP              0x20
/* CCamera, CCam */
#define CAM_WIDESCREEN       0x3D
#define CAM_ACTIVE           0x59
#define CAM_WATER_DIST       0xE4
#define CAM_WATER_HEIGHT     0xE8
#define CAM_LOD_MULT         0xF0
#define CAM_GEN_MULT         0xF4
#define CAM_CAMS             0x174
#define CAM_SIZE             0x238
#define CAM_FADE_ALPHA       0xBFC
#define CCAM_MODE            0x0C
#define CCAM_VANGLE          0xAC
#define CCAM_HANGLE          0xBC
/* CPad */
#define PAD_NEW_CROSS        0x20
#define PAD_OLD_CROSS        0x50
#define PAD_LAST_TOUCHED     0x120
/* model infos */
#define MI_REFCOUNT          0x08
#define MI_VT_MODEL_TYPE     0x10
#define PMI_PED_TYPE         0x28
#define PMI_STAT             0x2C
#define SI_SIZE              0x14
#define SI_LOAD_STATE        0x10

#define MODEL_INFOS          (*(uint8_t ***)0x403DA7)     /* SA-MP R5 moves the array */
#define MODEL_COUNT          (*(int32_t *)0x407106)        /* limit adjusters raise it */
#define STREAMING_INFO       (*(uint8_t **)0x409D45)
#define PICKUPS_START        (*(uint8_t **)0x48ADC3)       /* limit adjusters move them */
#define PICKUPS_END          (*(uint8_t **)0x4590E4)
#define PICKUP_SIZE          (*(uint8_t *)0x4590E1)
#define THE_CAMERA           ((uint8_t *)0xB6F028)
#define PADS                 ((uint8_t *)0xB73458)
#define PAD_SIZE             0x134
#define PLAYERS              ((uint8_t *)0xB7CD98)         /* CPlayerInfo, 0x190 each */
#define PINFO_MAX_HEALTH     0x14F
#define GAME_TIME_MS              (*(uint32_t *)0xB7CB7C)
#define FRAME_COUNTER        (*(uint32_t *)0xB7CB4C)
#define NEW_WEATHER          (*(int16_t *)0xC8131C)
#define FORCED_WEATHER       (*(uint16_t *)0xC81318)
#define RAIN                 (*(float *)0xC812E4)
#define UNDERWATERNESS       (*(float *)0xC8132C)
#define DAY_NIGHT_BALANCE    (*(float *)0x8D12C0)
#define MOUSE_ACCEL_H        (*(float *)0xB6EC1C)
#define USE_MOUSE_3RD_PERSON (*(uint8_t *)0xB6EC2E)
#define ON_MISSION_FLAG      (*(int32_t *)0xA476AC)
#define SCRIPT_SPACE         ((uint8_t *)0xA49960)
#define CUTSCENE_RUNNING     (*(uint8_t *)0xB5F851)
#define CHEATS_ACTIVE        ((uint8_t *)0x969130)
#define EXIT_ENTER_STATE     (*(int32_t *)0x96A7CC)
#define HUD_NO_RADAR         (*(uint8_t *)0xBAA3FB)
#define HUD_ITEM_TO_FLASH    (*(int32_t *)0xBAB1DC)
#define MENU_MANAGER         ((uint8_t *)0xBA6748)         /* hud on at +0x21, radar mode at +0x24 */
#define REPLAY_MODE          (*(uint8_t *)0xA43088)
#define TAKE_PHOTO           (*(uint8_t *)0xC8A7C1)
#define DISPLAY_HUD          (*(uint8_t *)0xA444A0)
#define RADAR_HIDDEN_FRAME   (*(uint32_t *)0xB6EC40)
#define CONTROLS             ((uint8_t *)0xB70198)         /* CControllerAction[59] at +0xB70, 0x20 each */
#define RS_GLOBAL            ((uint8_t *)0xC17040)
#define EXTRA_OBJECTS_DIR    (*(void **)0x8E48D0)
#define RESOURCE_MANAGER     ((void *)0xA485A8)
#define CAR_GENERATORS       ((uint8_t *)0xC27AD0)         /* 0x20 each, IPL id at +0x1D */
#define USED_OBJECTS         ((uint8_t *)0xA44B70)         /* 0x1C each, model at +0x18 */
#define CAR_GEN_SIZE         (*(int8_t *)0x6F32AA)
#define FX_SYSTEM            ((void *)0xA9AE00)

typedef void *(__cdecl *PoolGetFn)(int);
typedef int (__cdecl *PoolRefFn)(void *);
typedef void *(__cdecl *WeaponInfoFn)(int, int);
typedef int8_t (THISCALL *SkillFn)(void *, int);
typedef void (THISCALL *ClearTasksFn)(void *, int, int);
typedef int (THISCALL *CanSeeFn)(void *, void *, int);
typedef void (THISCALL *ObjectDamageFn)(void *, float, void *, void *, void *, int);
typedef float (THISCALL *BaseOfModelFn)(void *);
typedef void (__cdecl *ClumpAlphaFn)(void *, int);
typedef float (__cdecl *GroundZ3dFn)(float, float, float, uint8_t *, void **);
typedef float (__cdecl *GroundZFn)(float, float);
typedef int (__cdecl *VerticalLineFn)(const float *, float, void *, void **, int, int, int, int, int, int, void *);
typedef void (THISCALL *SetPosnFn)(void *, float, float, float);
typedef void *(__cdecl *ModelByNameFn)(const char *, int *);
typedef void *(THISCALL *FindItemFn)(void *, const char *, uint32_t *, uint32_t *);
typedef void (__cdecl *RequestModelFn)(int, int);
typedef void (__cdecl *ModelFn)(int);
typedef void (__cdecl *VoidFn)(void);
typedef int (__cdecl *InImageFn)(int);
typedef void (THISCALL *AddResourceFn)(void *, int, int, void *);
typedef float *(__cdecl *TransformFn)(float *, const float *, const void *);
typedef void (__cdecl *PassTimeFn)(uint32_t);
typedef void (THISCALL *TargetVectorFn)(void *, float, float, float, float, float *, float *);
typedef float (__cdecl *AtanFn)(float, float);
typedef float (THISCALL *HeadingFn)(void *);
typedef int (THISCALL *PadBoolFn)(void *);
typedef int (THISCALL *KeyDownFn)(void *, int);
typedef int16_t (THISCALL *PadStateFn)(void *, int, int);
typedef void *(__cdecl *ObjectCreateFn)(int);
typedef void (THISCALL *EntityFn)(void *);
typedef void (THISCALL *OrientFn)(void *, float, float, float);
typedef void (__cdecl *ClearSpaceFn)(const float *, void *);
typedef void (__cdecl *WorldAddFn)(void *);
typedef void *(THISCALL *SimplestTaskFn)(void *);
typedef void (THISCALL *QuatFn)(void *);
typedef void (THISCALL *MatFromVecFn)(void *, void *, const float *, const float *);
typedef uint8_t (THISCALL *ModelTypeFn)(void *);   /* a byte in al */
typedef void (THISCALL *PlayHornFn)(void *);
typedef int (THISCALL *TaskIdFn)(void *);
typedef void *(THISCALL *SubTaskFn)(void *);

#define GetPedAt             ((PoolGetFn)0x54FF90)
#define GetVehAt             ((PoolGetFn)0x54FFF0)
#define GetObjectAt          ((PoolGetFn)0x550050)
#define GetPedRef            ((PoolRefFn)0x54FF60)
#define GetVehRef            ((PoolRefFn)0x54FFC0)
#define GetObjectRef         ((PoolRefFn)0x550020)
#define GetWeaponInfo        ((WeaponInfoFn)0x743C60)
#define GetWeaponSkill       ((SkillFn)0x5E3B60)
#define ClearTasks           ((ClearTasksFn)0x601420)
#define CanSeeEntity         ((CanSeeFn)0x5E1660)
#define ObjectDamage         ((ObjectDamageFn)0x5A0D90)
#define BaseOfModel          ((BaseOfModelFn)0x536BE0)
#define SetClumpAlpha        ((ClumpAlphaFn)0x732B00)
#define FindGroundZ3d        ((GroundZ3dFn)0x5696C0)
#define FindGroundZ          ((GroundZFn)0x569660)
#define ProcessVerticalLine  ((VerticalLineFn)0x5674E0)
#define SetPosn              ((SetPosnFn)0x420B80)
#define GetModelByName       ((ModelByNameFn)0x4C5940)
#define DirFindItem          ((FindItemFn)0x5324A0)
#define RequestModel         ((RequestModelFn)0x4087E0)
#define RemoveModel          ((ModelFn)0x4089A0)
#define RemoveAllUnused      ((VoidFn)0x40CF80)
#define IsObjectInCdImage    ((InImageFn)0x407800)
#define AddToResources       ((AddResourceFn)0x4704B0)
#define RwV3dTransformPoint  ((TransformFn)0x7EDD60)
#define PassTime             ((PassTimeFn)0x4414C0)
#define Find3rdPersonTarget  ((TargetVectorFn)0x514970)
#define GetATanOfXY          ((AtanFn)0x53CC70)
#define GetHeading           ((HeadingFn)0x441DB0)
#define PadGetTarget         ((PadBoolFn)0x540670)
#define KeyJustDown          ((KeyDownFn)0x52E450)
#define GetPadState          ((PadStateFn)0x485B10)
#define ObjectCreate         ((ObjectCreateFn)0x5A1F60)
#define SetOrientation       ((OrientFn)0x439A80)
#define UpdateRW             ((EntityFn)0x446F90)
#define UpdateRwFrame        ((EntityFn)0x532B00)
#define SetupBigBuilding     ((EntityFn)0x533150)
#define ClearSpaceForMission ((ClearSpaceFn)0x5D3390)
#define WorldAdd             ((WorldAddFn)0x563220)
#define SimplestActiveTask   ((SimplestTaskFn)0x6819D0)
#define QuatNormalise        ((QuatFn)0x4D1610)
#define CreateMatFromVec     ((MatFromVecFn)0x49E950)

/* ---- parameters ------------------------------------------------------ */

static void store_int(Script *s, int32_t v) { ScriptParams[0].i = v; script_store(s, 1); }
static void store_float(Script *s, float v) { ScriptParams[0].f = v; script_store(s, 1); }
static void store_ptr(Script *s, const void *p) { store_int(s, (int32_t)(uintptr_t)p); }

static void store_floats(Script *s, const float *f, int n)
{
    int i;
    for (i = 0; i < n; i++)
        ScriptParams[i].f = f[i];
    script_store(s, n);
}

static void store_ints(Script *s, const int32_t *v, int n)
{
    int i;
    for (i = 0; i < n; i++)
        ScriptParams[i].i = v[i];
    script_store(s, n);
}

static uint8_t *ped_arg(Script *s) { return GetPedAt(script_int(s)); }
static uint8_t *veh_arg(Script *s) { return GetVehAt(script_int(s)); }
static uint8_t *obj_arg(Script *s) { return GetObjectAt(script_int(s)); }

static float fsqrt(float x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

static float *entity_pos(uint8_t *e)
{
    uint8_t *m = AT(e, ENT_MATRIX, uint8_t *);
    return m ? (float *)(m + 0x30) : (float *)(e + PLACEABLE_POS);
}

static float dist_sq(const float *a, const float *b)
{
    float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return x * x + y * y + z * z;
}

static int model_count(void) { return MODEL_COUNT < 10000 ? 20000 : MODEL_COUNT; }

static uint8_t *model_info(int id)
{
    return id >= 0 && id < model_count() ? MODEL_INFOS[id] : NULL;
}

static uint8_t *active_cam(void)
{
    return THE_CAMERA + CAM_CAMS + THE_CAMERA[CAM_ACTIVE] * CAM_SIZE;
}

/* ---- cars ------------------------------------------------------------ */

static int8_t __stdcall op_0E00(Script *s)
{
    uint8_t *v = veh_arg(s);
    int mode = -1;
    if (v) {
        uint16_t a = AT(v, VEH_ALARM, uint16_t);
        mode = a == 0 ? 0 : a == 0xFFFF ? 1 : 2;
    }
    store_int(s, mode);
    return 0;
}

static int8_t __stdcall op_0E61(Script *s)
{
    uint8_t *v;
    int mode;
    script_collect(s, 2);
    v = GetVehAt(ScriptParams[0].i);
    mode = ScriptParams[1].i;
    if (v && mode >= 0 && mode <= 2)
        AT(v, VEH_ALARM, uint16_t) = mode == 0 ? 0 : mode == 1 ? 0xFFFF : 0xFFFE;
    return 0;
}

static int8_t __stdcall op_0E08(Script *s)
{
    uint8_t *v = veh_arg(s);
    script_cond(s, v && AT(v, VEH_CREATED_BY, uint8_t) == 2);
    return 0;
}

static int8_t __stdcall op_0E09(Script *s)
{
    uint8_t *v = veh_arg(s);
    if (v)
        AT(v, VEH_CREATED_BY, uint8_t) = 2;
    return 0;
}

static int8_t trailer_link(Script *s, int off)
{
    uint8_t *v = veh_arg(s), *other = v ? AT(v, off, uint8_t *) : NULL;
    int ref = other ? GetVehRef(other) : -1;
    store_int(s, ref);
    script_cond(s, ref != -1);
    return 0;
}

static int8_t __stdcall op_0E59(Script *s) { return trailer_link(s, VEH_TRAILER); }
static int8_t __stdcall op_0E5A(Script *s) { return trailer_link(s, VEH_TRACTOR); }

static int8_t __stdcall op_0E5F(Script *s)
{
    uint8_t *v = veh_arg(s);
    if (v) {
        AT(v, VEH_HORN_COUNTER, uint32_t) = 1;
        (*(PlayHornFn *)(*(uint8_t **)v + VEH_VT_PLAY_HORN))(v);
    }
    return 0;
}

static int8_t __stdcall op_0E65(Script *s)
{
    uint8_t *v = veh_arg(s);
    float f = v ? AT(v, PHYS_DAMAGE, float) : 0;
    store_float(s, f);
    script_cond(s, f > 0);
    return 0;
}

static int8_t __stdcall op_0E66(Script *s)
{
    static const float zero[3];
    uint8_t *v = veh_arg(s);
    store_floats(s, v ? (float *)(v + PHYS_LAST_COLLISION) : zero, 3);
    return 0;
}

static int8_t __stdcall op_0E82(Script *s)
{
    uint8_t *v;
    int node;
    script_collect(s, 2);
    v = GetVehAt(ScriptParams[0].i);
    node = ScriptParams[1].i;
    script_cond(s, v && node >= 0 && node < 25 && AT(v, AUTO_CAR_NODES + node * 4, void *) != NULL);
    return 0;
}

static int really_in_air(uint8_t *e) { return e && !(e[PHYS_FLAGS1] & 3); }

/* A ped in a vehicle has no physics of its own (its flags read as in the
 * air), so its vehicle answers. */
static int8_t __stdcall op_0E92(Script *s)
{
    uint8_t *p = ped_arg(s), *v = p && (p[PED_FLAGS_IN_VEHICLE] & 1) ? AT(p, PED_VEHICLE, uint8_t *) : NULL;
    script_cond(s, really_in_air(v ? v : p));
    return 0;
}
static int8_t __stdcall op_0E93(Script *s) { script_cond(s, really_in_air(veh_arg(s))); return 0; }
static int8_t __stdcall op_0E94(Script *s) { script_cond(s, really_in_air(obj_arg(s))); return 0; }

static int8_t proofs(Script *s, uint8_t *e)
{
    uint8_t f = e ? e[PHYS_FLAGS2] : 0;
    int32_t v[5] = { !!(f & 0x04), !!(f & 0x08), !!(f & 0x80), !!(f & 0x10), !!(f & 0x20) };
    store_ints(s, v, 5);
    return 0;
}

static int8_t __stdcall op_0EAC(Script *s) { return proofs(s, ped_arg(s)); }
static int8_t __stdcall op_0EAD(Script *s) { return proofs(s, veh_arg(s)); }
static int8_t __stdcall op_0EAE(Script *s) { return proofs(s, obj_arg(s)); }

static int8_t set_coords_simple(Script *s, PoolGetFn get)
{
    uint8_t *e, *m;
    script_collect(s, 4);
    e = get(ScriptParams[0].i);
    if (e && (m = AT(e, ENT_MATRIX, uint8_t *)) != NULL) {
        AT(m, 0x30, float) = ScriptParams[1].f;
        AT(m, 0x34, float) = ScriptParams[2].f;
        AT(m, 0x38, float) = ScriptParams[3].f;
    }
    return 0;
}

static int8_t __stdcall op_0E32(Script *s) { return set_coords_simple(s, GetPedAt); }
static int8_t __stdcall op_0EB4(Script *s) { return set_coords_simple(s, GetVehAt); }

static int8_t random_seed(Script *s, uint8_t *e)
{
    store_int(s, e ? AT(e, ENT_RANDOM_SEED, uint16_t) : 0);
    return 0;
}

static int8_t __stdcall op_0EC8(Script *s) { return random_seed(s, ped_arg(s)); }
static int8_t __stdcall op_0EC9(Script *s) { return random_seed(s, veh_arg(s)); }
static int8_t __stdcall op_0ECA(Script *s) { return random_seed(s, obj_arg(s)); }

static int8_t dont_delete(Script *s, PoolGetFn get, int off)
{
    uint8_t *e;
    uint32_t t;
    script_collect(s, 2);
    e = get(ScriptParams[0].i);
    t = (uint32_t)ScriptParams[1].i + GAME_TIME_MS;
    if (e && AT(e, off, uint32_t) < t)
        AT(e, off, uint32_t) = t;
    return 0;
}

static int8_t __stdcall op_0ECC(Script *s) { return dont_delete(s, GetPedAt, PED_NEED_UNTIL); }
static int8_t __stdcall op_0ECD(Script *s) { return dont_delete(s, GetVehAt, VEH_NEED_UNTIL); }

static int8_t __stdcall op_0ECF(Script *s)
{
    uint8_t *v = veh_arg(s);
    store_int(s, v && AT(v, VEH_HEALTH, float) <= 0 ? (int32_t)(GAME_TIME_MS - AT(v, VEH_BLOWN_UP_AT, uint32_t)) : -1);
    return 0;
}

static int8_t __stdcall op_0EF5(Script *s)
{
    uint8_t *v = veh_arg(s);
    script_cond(s, v && (v[VEH_OWNED_FLAGS] & 2));
    return 0;
}

static int8_t __stdcall op_0EF6(Script *s)
{
    uint8_t *v;
    script_collect(s, 2);
    if ((v = GetVehAt(ScriptParams[0].i)) != NULL) {
        if (ScriptParams[1].i == 1)
            v[VEH_OWNED_FLAGS] |= 2;
        else
            v[VEH_OWNED_FLAGS] &= ~2;
    }
    return 0;
}

static uint8_t *handling(Script *s)
{
    uint8_t *v = veh_arg(s);
    return v ? AT(v, VEH_HANDLING, uint8_t *) : NULL;
}

static int8_t __stdcall op_0EF9(Script *s) { uint8_t *h = handling(s); store_int(s, h ? h[HAND_ANIM_GROUP] : 0); return 0; }
static int8_t __stdcall op_0EFB(Script *s) { uint8_t *h = handling(s); script_cond(s, h && (h[HAND_MODEL_FLAGS2] & 8)); return 0; }
static int8_t __stdcall op_0EFC(Script *s) { uint8_t *h = handling(s); store_int(s, h ? AT(h, HAND_VALUE, int32_t) : 0); return 0; }

static int8_t __stdcall op_0EFD(Script *s)
{
    uint8_t *v = veh_arg(s);
    float f[2] = { v ? AT(v, VEH_GAS, float) : 0, v ? AT(v, VEH_BRAKE, float) : 0 };
    store_floats(s, f, 2);
    return 0;
}

static int8_t contact_surface(Script *s, uint8_t *e) { store_int(s, e ? e[PHYS_CONTACT_SURFACE] : 0); return 0; }
static int8_t contact_light(Script *s, uint8_t *e) { store_float(s, e ? AT(e, PHYS_BRIGHTNESS, float) : 0); return 0; }

static int8_t __stdcall op_0E8E(Script *s) { return contact_surface(s, ped_arg(s)); }
static int8_t __stdcall op_0E8F(Script *s) { return contact_light(s, ped_arg(s)); }
static int8_t __stdcall op_0E90(Script *s) { return contact_surface(s, veh_arg(s)); }
static int8_t __stdcall op_0E91(Script *s) { return contact_light(s, veh_arg(s)); }

/* ---- chars ----------------------------------------------------------- */

static int8_t __stdcall op_0E0A(Script *s)
{
    uint8_t *p = ped_arg(s);
    script_cond(s, p && AT(p, PED_CREATED_BY, uint32_t) == 2);
    return 0;
}

static int8_t __stdcall op_0E0B(Script *s)
{
    uint8_t *p = ped_arg(s);
    if (p)
        AT(p, PED_CREATED_BY, uint32_t) = 2;
    return 0;
}

static int8_t __stdcall op_0E24(Script *s)
{
    uint8_t *p, col[0x2C];
    void *hit;
    float *pos;
    int ground, bright, fade;
    script_collect(s, 4);
    p = GetPedAt(ScriptParams[0].i);
    ground = ScriptParams[1].i;
    bright = ScriptParams[2].i;
    fade = ScriptParams[3].i;
    if (!p)
        return 0;
    if (fade && AT(p, ENT_RWOBJECT, void *))
        SetClumpAlpha(AT(p, ENT_RWOBJECT, void *), 0);
    pos = entity_pos(p);
    if (ground) {
        uint8_t found = 0;
        void *e;
        float z = FindGroundZ3d(pos[0], pos[1], pos[2], &found, &e);
        if (found)
            SetPosn(p, pos[0], pos[1], BaseOfModel(p) + z);
    }
    if (bright && ProcessVerticalLine(pos, -100.0f, col, &hit, 1, 0, 0, 1, 0, 0, NULL)) {
        float day = (float)(col[0x25] & 0xF), night = (float)(col[0x25] >> 4);
        if (night < 0.001f)
            night = day * 0.9f;
        else if (day < 0.001f && (day = night * 1.1f) > 1.0f)
            day = 1.0f;
        AT(p, PHYS_BRIGHTNESS, float) = day * 0.033333334f * (1.0f - DAY_NIGHT_BALANCE) +
                                        night * 0.033333334f * DAY_NIGHT_BALANCE;
    }
    return 0;
}

static int8_t __stdcall op_0E5C(Script *s)
{
    uint8_t *p = ped_arg(s);
    float max, pct = 0;
    if (p) {
        int type = AT(p, PED_TYPE, int32_t);
        max = type <= 1 ? (float)PLAYERS[type * 0x190 + PINFO_MAX_HEALTH] : AT(p, PED_MAX_HEALTH, float);
        if (max > 0)
            pct = AT(p, PED_HEALTH, float) / max * 100.0f;
    }
    store_float(s, pct);
    return 0;
}

static uint8_t *active_weapon(uint8_t *p)
{
    return p + PED_WEAPONS + p[PED_ACTIVE_SLOT] * 0x1C;
}

static int8_t __stdcall op_0E83(Script *s)
{
    uint8_t *p = ped_arg(s);
    void *wi = NULL;
    if (p) {
        int type = AT(active_weapon(p), 0, int32_t);
        wi = GetWeaponInfo(type, GetWeaponSkill(p, type));
    }
    store_ptr(s, wi);
    script_cond(s, wi != NULL);
    return 0;
}

static int8_t __stdcall op_0E8B(Script *s) { uint8_t *p = ped_arg(s); store_int(s, p ? AT(active_weapon(p), 4, int32_t) : 0); return 0; }
static int8_t __stdcall op_0E8C(Script *s) { uint8_t *p = ped_arg(s); store_int(s, p ? AT(active_weapon(p), 8, int32_t) : 0); return 0; }

static int8_t clear_tasks(Script *s, int primary, int secondary)
{
    uint8_t *p = ped_arg(s);
    if (p && AT(p, PED_INTELLIGENCE, void *))
        ClearTasks(AT(p, PED_INTELLIGENCE, void *), primary, secondary);
    return 0;
}

static int8_t __stdcall op_0E96(Script *s) { return clear_tasks(s, 1, 0); }
static int8_t __stdcall op_0E97(Script *s) { return clear_tasks(s, 0, 1); }

static int8_t __stdcall op_0EA4(Script *s) { uint8_t *p = ped_arg(s); script_cond(s, p && AT(p, PED_FIRE, void *)); return 0; }

/* The closest cop to pos: ped type 6, or models 280-288 (the game gives
 * some police station cops another type). */
static uint8_t *closest_cop(const float *pos, uint8_t *self, float radius, int alive, int in_car,
                            int on_foot, int seen)
{
    uint8_t *pool = *(uint8_t **)0xB74490, *best = NULL;
    float best_d = 0;
    int i, n = AT(pool, 8, int32_t);
    for (i = 0; i < n; i++) {
        uint8_t *p;
        int model;
        float d;
        if (AT(pool, 4, int8_t *)[i] < 0)
            continue;
        p = AT(pool, 0, uint8_t *) + i * 0x7C4;
        if (p == self)
            continue;
        if (AT(p, PED_VEHICLE, void *) ? (on_foot && !in_car) : (in_car && !on_foot))
            continue;
        if (alive && AT(p, PED_HEALTH, float) <= 0)
            continue;
        d = dist_sq(entity_pos(p), pos);
        if (radius < 1000.0f && d > radius * radius)
            continue;
        model = AT(p, ENT_MODEL, int16_t);
        if (AT(p, PED_TYPE, int32_t) != 6 && (model < 280 || model > 288))
            continue;
        if (seen && !CanSeeEntity(p, self, 0))
            continue;
        if (best && best_d < d)
            continue;
        best = p;
        best_d = d;
    }
    return best;
}

static int8_t found_ped(Script *s, uint8_t *p)
{
    store_int(s, p ? GetPedRef(p) : -1);
    script_cond(s, p != NULL);
    return 0;
}

static int8_t __stdcall op_0EA5(Script *s)
{
    uint8_t *p;
    script_collect(s, 6);
    p = GetPedAt(ScriptParams[0].i);
    if (!p)
        return found_ped(s, NULL);
    return found_ped(s, closest_cop(entity_pos(p), p, ScriptParams[1].f, ScriptParams[2].i,
                                    ScriptParams[3].i, ScriptParams[4].i, ScriptParams[5].i));
}

static int8_t __stdcall op_0EA6(Script *s)
{
    float pos[3];
    script_collect(s, 7);
    pos[0] = ScriptParams[0].f;
    pos[1] = ScriptParams[1].f;
    pos[2] = ScriptParams[2].f;
    return found_ped(s, closest_cop(pos, NULL, ScriptParams[3].f, ScriptParams[4].i, ScriptParams[5].i,
                                    ScriptParams[6].i, 0));
}

static int8_t __stdcall op_0EAA(Script *s) { uint8_t *p = ped_arg(s); if (p) AT(p, PED_STATE, int32_t) = PEDSTATE_ARRESTED; return 0; }
static int8_t __stdcall op_0EAB(Script *s) { uint8_t *p = ped_arg(s); store_int(s, p ? AT(p, PED_STATE, int32_t) : 0); return 0; }
static int8_t __stdcall op_0ECB(Script *s) { uint8_t *p = ped_arg(s); store_int(s, p ? AT(p, PED_MOVE_STATE, int32_t) : 0); return 0; }

static int8_t __stdcall op_0EAF(Script *s)
{
    uint8_t *p = ped_arg(s), *d = p ? AT(p, PED_PLAYER_DATA, uint8_t *) : NULL;
    script_cond(s, !d || (d[PDATA_FLAGS] & 0x10));
    return 0;
}

static uint8_t *ped_stats(Script *s)
{
    uint8_t *p = ped_arg(s);
    return p ? AT(p, PED_STATS, uint8_t *) : NULL;
}

static int8_t __stdcall op_0EB1(Script *s) { uint8_t *st = ped_stats(s); store_int(s, st ? AT(st, 0, int32_t) : -1); return 0; }
static int8_t __stdcall op_0EFA(Script *s) { uint8_t *st = ped_stats(s); store_int(s, st ? st[0x24] : 0); return 0; }

static int8_t __stdcall op_0ECE(Script *s)
{
    uint8_t *p = ped_arg(s);
    int state = p ? AT(p, PED_STATE, int32_t) : 0;
    /* CPed::IsAlive: not dying (54) or dead (55) */
    store_int(s, p && (state == 54 || state == 55) ? (int32_t)(GAME_TIME_MS - AT(p, PED_DEATH_TIME, uint32_t)) : -1);
    return 0;
}

static int task_id(uint8_t *t) { return (*(TaskIdFn *)(*(uint8_t **)t + 0x10))(t); }
static uint8_t *sub_task(uint8_t *t) { return (*(SubTaskFn *)(*(uint8_t **)t + 0x08))(t); }

/* Walks a ped's primary then secondary task trees for a task id. */
static void *find_task(uint8_t *p, int id)
{
    uint8_t *intel = p ? AT(p, PED_INTELLIGENCE, uint8_t *) : NULL;
    int i;
    if (!intel)
        return NULL;
    for (i = 0; i < 10; i++) {
        uint8_t *t = AT(intel, 4 + i * 4, uint8_t *);    /* m_TaskMgr: primary[5], secondary[5] */
        while (t) {
            if (task_id(t) == id)
                return t;
            t = sub_task(t);
        }
    }
    return NULL;
}

/* CLEO+'s AI flags, from the same walk of the task trees it caches once a
 * frame: here they are read when asked. */
enum {
    AI_USING_GUN = 1, AI_FIGHTING = 2, AI_FALLEN = 4, AI_ENTERING_CAR = 8, AI_EXITING_CAR = 0x10,
    AI_ANIM_PRIMARY = 0x20, AI_ANIM_SECONDARY = 0x40, AI_KILLING = 0x80, AI_ROOT_UNIMPORTANT = 0x100,
};

static int unimportant(int id, uint8_t *p)
{
    static const int16_t ids[] = {
        912, 709, 725, 711, 401, 917, 1201, 1207, 1217, 907, 0, 1, 203, 421, 1214, 1216, 418, 417,
        508, 514, 510, 919, 509, 256,
    };
    size_t i;
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++)
        if (id == ids[i])
            return 1;
    /* fleeing, but not really running */
    return AT(p, PED_MOVE_STATE, int32_t) <= 4 && (id == 927 || id == 909 || id == 908 || id == 910 || id == 911);
}

static int ai_flags(uint8_t *p, void **target)
{
    uint8_t *intel = p ? AT(p, PED_INTELLIGENCE, uint8_t *) : NULL;
    int flags = 0, n = 0, i;
    *target = NULL;
    if (!intel)
        return 0;
    for (i = 0; i < 10; i++) {
        uint8_t *t;
        for (t = AT(intel, 4 + i * 4, uint8_t *); t; t = sub_task(t)) {
            int id = task_id(t), off = 0;
            switch (id) {
            case 1101: off = 4; break;                          /* arrest ped */
            case 1000: off = 16; break;                         /* kill ped on foot */
            case 1100: case 506: case 507: off = 8; break;      /* simple arrest, kill / hurt with car */
            case 1046: case 1105: case 505: off = 12; break;    /* from a boat, kill criminal, hit with car */
            case 208: case 209: flags |= AI_FALLEN; break;
            case 1031: flags |= AI_USING_GUN; break;
            case 1030: flags |= AI_FIGHTING; break;
            case 700: case 701: flags |= AI_ENTERING_CAR; break;
            case 704: case 714: case 716: flags |= AI_EXITING_CAR; break;
            case 401: flags |= i >= 5 ? AI_ANIM_SECONDARY : AI_ANIM_PRIMARY; break;
            }
            if (++n == 1 && !(flags & AI_KILLING)) {
                uint8_t *sub = id == 243 ? sub_task(t) : t;    /* be in group: look at what it does */
                if (sub && unimportant(task_id(sub), p))
                    flags |= AI_ROOT_UNIMPORTANT;
            }
            if (off) {
                flags |= AI_KILLING;
                *target = AT(t, off, void *);
            }
        }
    }
    return flags;
}

static int8_t ai_cond(Script *s, int mask)
{
    void *target;
    script_cond(s, (ai_flags(ped_arg(s), &target) & mask) != 0);
    return 0;
}

static int8_t __stdcall op_0E46(Script *s) { return ai_cond(s, AI_USING_GUN); }
static int8_t __stdcall op_0E47(Script *s) { return ai_cond(s, AI_FIGHTING); }
static int8_t __stdcall op_0E48(Script *s) { return ai_cond(s, AI_FALLEN); }
static int8_t __stdcall op_0E49(Script *s) { return ai_cond(s, AI_ENTERING_CAR); }
static int8_t __stdcall op_0E4A(Script *s) { return ai_cond(s, AI_EXITING_CAR); }

/* includeAnims: 0 none, 1 primary, 2 secondary, 3 both */
static int playing_anim(int flags, int include)
{
    return (include == 1 && (flags & AI_ANIM_PRIMARY)) || (include == 2 && (flags & AI_ANIM_SECONDARY)) ||
           (include == 3 && (flags & (AI_ANIM_PRIMARY | AI_ANIM_SECONDARY)));
}

static int8_t __stdcall op_0E4B(Script *s)
{
    void *target;
    int flags;
    script_collect(s, 2);
    flags = ai_flags(GetPedAt(ScriptParams[0].i), &target);
    script_cond(s, playing_anim(flags, ScriptParams[1].i));
    return 0;
}

static int8_t __stdcall op_0E4C(Script *s)
{
    void *target;
    uint8_t *p;
    int flags;
    script_collect(s, 2);
    p = GetPedAt(ScriptParams[0].i);
    flags = ai_flags(p, &target);
    if (p && (flags & AI_ROOT_UNIMPORTANT) && !(flags & AI_KILLING))
        script_cond(s, playing_anim(flags, ScriptParams[1].i));
    else
        script_cond(s, p != NULL);
    return 0;
}

/* The ped a task is after; for the player with none, the ped it targets. */
static int8_t __stdcall op_0E44(Script *s)
{
    uint8_t *p = ped_arg(s), *e;
    void *target;
    int ref = -1;
    ai_flags(p, &target);
    e = target;
    if (!e && p && AT(p, PED_TYPE, int32_t) <= 1)
        e = AT(p, PPED_TARGETTED, uint8_t *);
    if (e && (e[0x36] & 7) == 3)    /* a ped */
        ref = GetPedRef(e);
    store_int(s, ref);
    script_cond(s, ref > 0);
    return 0;
}

static int8_t __stdcall op_0E42(Script *s)
{
    uint8_t *p;
    script_collect(s, 2);
    p = GetPedAt(ScriptParams[0].i);
    script_cond(s, find_task(p, ScriptParams[1].i) != NULL);
    return 0;
}

static int8_t __stdcall op_0E43(Script *s)
{
    uint8_t *p;
    void *t;
    script_collect(s, 2);
    p = GetPedAt(ScriptParams[0].i);
    t = find_task(p, ScriptParams[1].i);
    store_ptr(s, t);
    script_cond(s, t != NULL);
    return 0;
}

static int8_t __stdcall op_0EFF(Script *s)
{
    uint8_t *p = ped_arg(s), *intel = p ? AT(p, PED_INTELLIGENCE, uint8_t *) : NULL;
    void *t = intel ? SimplestActiveTask(intel + 4) : NULL;
    int32_t v[2] = { t ? task_id(t) : -1, t ? (int32_t)(uintptr_t)t : -1 };
    store_ints(s, v, 2);
    script_cond(s, t != NULL);
    return 0;
}

/* ---- objects --------------------------------------------------------- */

static int8_t __stdcall op_0E0C(Script *s)
{
    uint8_t *o = obj_arg(s);
    script_cond(s, o && (o[OBJ_TYPE] == OBJ_MISSION || o[OBJ_TYPE] == OBJ_MISSION2));
    return 0;
}

static int8_t __stdcall op_0E0D(Script *s)
{
    uint8_t *o = obj_arg(s);
    if (o)
        o[OBJ_TYPE] = OBJ_MISSION2;    /* not saved: MoonLoader's scripts are not the main script */
    return 0;
}

static int8_t __stdcall op_0E71(Script *s) { uint8_t *o = obj_arg(s); store_float(s, o ? BaseOfModel(o) : 0); return 0; }

static int8_t __stdcall op_0E95(Script *s)
{
    uint8_t *o;
    script_collect(s, 3);
    if ((o = GetObjectAt(ScriptParams[0].i)) != NULL)
        ObjectDamage(o, ScriptParams[1].f, NULL, NULL, NULL, ScriptParams[2].i);
    return 0;
}

static int8_t __stdcall op_0E01(Script *s)
{
    uint8_t *mi, *o;
    float x, y, z, pos[3];
    int model, offset, fix;
    script_collect(s, 6);
    model = ScriptParams[0].i;
    x = ScriptParams[1].f;
    y = ScriptParams[2].f;
    z = ScriptParams[3].f;
    offset = ScriptParams[4].i;
    fix = ScriptParams[5].i;
    if (model < 0)
        model = AT(USED_OBJECTS + -model * 0x1C, 0x18, int32_t);
    if (!(mi = model_info(model)) || !(o = ObjectCreate(model))) {
        store_int(s, -1);
        return 0;
    }
    mi[0x0C] = 0xFF;
    o[OBJ_TYPE] = OBJ_MISSION2;
    if (z <= -100.0f) {
        z = FindGroundZ(x, y);
    } else if (fix) {
        uint8_t found = 0;
        void *e;
        float gz = FindGroundZ3d(x, y, z, &found, &e);
        if (found)
            z = gz;
    }
    if (offset)
        z += BaseOfModel(o);
    SetPosn(o, x, y, z);
    SetOrientation(o, 0, 0, 0);
    UpdateRW(o);
    UpdateRwFrame(o);
    if ((*(void *(THISCALL **)(void *))(*(uint8_t **)mi + 0x0C))(mi))     /* AsLodAtomicModelInfoPtr */
        SetupBigBuilding(o);
    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
    ClearSpaceForMission(pos, o);
    WorldAdd(o);
    store_int(s, GetObjectRef(o));
    return 0;
}

static int8_t __stdcall op_0E02(Script *s)
{
    int ref = script_int(s);
    uint8_t *gen = CAR_GENERATORS + ref * 0x20;
    if (ref < 0 || ref >= 500)
        return 0;
    if (CAR_GEN_SIZE >= 0x28)
        AT(gen, 0x24, uint32_t) = 0xFFFFFFFF;    /* a limit adjuster's wider generators */
    else
        gen[0x1D] = 0xFF;
    return 0;
}

/* ---- the world ------------------------------------------------------- */

static int8_t __stdcall op_0E04(Script *s) { store_int(s, NEW_WEATHER); return 0; }
static int8_t __stdcall op_0E05(Script *s) { NEW_WEATHER = (int16_t)script_int(s); return 0; }
static int8_t __stdcall op_0E06(Script *s) { store_float(s, RAIN); return 0; }
static int8_t __stdcall op_0E07(Script *s) { RAIN = script_float(s); return 0; }
static int8_t __stdcall op_0E6C(Script *s) { store_float(s, DAY_NIGHT_BALANCE); return 0; }
static int8_t __stdcall op_0E6D(Script *s) { store_float(s, UNDERWATERNESS); return 0; }

static int8_t __stdcall op_0EB0(Script *s)
{
    store_int(s, FORCED_WEATHER == 0xFFFF ? -1 : FORCED_WEATHER);
    script_cond(s, FORCED_WEATHER != 0xFFFF);
    return 0;
}

static int8_t __stdcall op_0E0F(Script *s)
{
    float xy[2];
    script_collect(s, 2);
    xy[0] = ScriptParams[0].f / ((float)AT(RS_GLOBAL, 4, int32_t) / ((float)AT(RS_GLOBAL, 8, int32_t) * 1.33333333f));
    xy[1] = ScriptParams[1].f / 1.07142857f;
    store_floats(s, xy, 2);
    return 0;
}

static int8_t __stdcall op_0E1D(Script *s) { script_cond(s, ON_MISSION_FLAG && SCRIPT_SPACE[ON_MISSION_FLAG]); return 0; }

static int8_t __stdcall op_0F16(Script *s)
{
    int v = script_int(s);
    if (ON_MISSION_FLAG)
        SCRIPT_SPACE[ON_MISSION_FLAG] = (uint8_t)v;
    return 0;
}

static int8_t __stdcall op_0E23(Script *s) { store_float(s, MOUSE_ACCEL_H); return 0; }
static int8_t __stdcall op_0E25(Script *s) { script_cond(s, CUTSCENE_RUNNING); return 0; }
static int8_t __stdcall op_0EB7(Script *s) { script_cond(s, THE_CAMERA[CAM_WIDESCREEN]); return 0; }

static int8_t __stdcall op_0E26(Script *s)
{
    uint8_t *wi;
    script_collect(s, 2);
    wi = GetWeaponInfo(ScriptParams[0].i, 1);
    script_cond(s, wi && AT(wi, WI_FIRE, int32_t) == ScriptParams[1].i);
    return 0;
}

static int8_t __stdcall op_0E5D(Script *s)
{
    int i = script_int(s);
    script_cond(s, i >= 0 && i < 92 && CHEATS_ACTIVE[i]);
    return 0;
}

static int8_t __stdcall op_0EB8(Script *s)
{
    script_cond(s, EXIT_ENTER_STATE != 1 && EXIT_ENTER_STATE != 2 && !HUD_NO_RADAR &&
                   AT(MENU_MANAGER, 0x24, int32_t) != 2 && (HUD_ITEM_TO_FLASH != 8 || (FRAME_COUNTER & 8)) &&
                   REPLAY_MODE != 1 && !TAKE_PHOTO && RADAR_HIDDEN_FRAME != FRAME_COUNTER);
    return 0;
}

static int8_t __stdcall op_0EB9(Script *s)
{
    script_cond(s, MENU_MANAGER[0x21] && DISPLAY_HUD && !THE_CAMERA[CAM_WIDESCREEN] && REPLAY_MODE != 1 &&
                   !TAKE_PHOTO && RADAR_HIDDEN_FRAME != FRAME_COUNTER);
    return 0;
}

static int8_t __stdcall op_0EBB(Script *s) { PassTime((uint32_t)script_int(s)); return 0; }

static int8_t __stdcall op_0EC7(Script *s)
{
    float a = AT(THE_CAMERA, CAM_FADE_ALPHA, float);
    store_float(s, a);
    script_cond(s, a > 0);
    return 0;
}

/* ---- pickups --------------------------------------------------------- */

static uint8_t *pickup_at(int handle)
{
    int i = handle & 0xFFFF;
    uint8_t *p;
    if (handle == -1 || i >= (PICKUPS_END - PICKUPS_START) / PICKUP_SIZE)
        return NULL;
    p = PICKUPS_START + i * PICKUP_SIZE;
    return AT(p, 0x1A, uint16_t) == (uint16_t)(handle >> 16) ? p : NULL;
}

static int8_t __stdcall op_0E33(Script *s)
{
    float at[3];
    int i, n, only, found = 0, ref = 0;
    script_collect(s, 4);
    at[0] = ScriptParams[0].f;
    at[1] = ScriptParams[1].f;
    at[2] = ScriptParams[2].f;
    only = ScriptParams[3].i;
    n = (PICKUPS_END - PICKUPS_START) / PICKUP_SIZE;
    for (i = 0; i < n; i++) {
        uint8_t *p = PICKUPS_START + i * PICKUP_SIZE;
        float pos[3] = { AT(p, 0x10, int16_t) / 8.0f, AT(p, 0x12, int16_t) / 8.0f, AT(p, 0x14, int16_t) / 8.0f };
        if (dist_sq(pos, at) < 0.25f && (!only || p[0x1C])) {
            ref = i | AT(p, 0x1A, uint16_t) << 16;
            found = 1;
            break;
        }
    }
    store_int(s, ref);
    script_cond(s, found);
    return 0;
}

static int8_t __stdcall op_0E34(Script *s)
{
    uint8_t *p = pickup_at(script_int(s));
    store_int(s, p ? AT(p, 0x18, int16_t) : -1);
    script_cond(s, p != NULL);
    return 0;
}

static int8_t __stdcall op_0E38(Script *s)
{
    uint8_t *p = pickup_at(script_int(s));
    store_ptr(s, p);
    script_cond(s, p != NULL);
    return 0;
}

static int8_t __stdcall op_0E39(Script *s)
{
    uint8_t *p = pickup_at(script_int(s));
    store_int(s, p ? p[0x1C] : -1);
    script_cond(s, p != NULL);
    return 0;
}

/* ---- input ----------------------------------------------------------- */

static int16_t buttons_last[2][20];

static int8_t __stdcall op_0E3E(Script *s)
{
    int pad, id;
    script_collect(s, 2);
    pad = ScriptParams[0].i;
    id = ScriptParams[1].i;
    script_cond(s, pad >= 0 && pad < 2 && id >= 0 && id < 20 && GetPadState(s, pad, id) && !buttons_last[pad][id]);
    return 0;
}

static int8_t __stdcall op_0E67(Script *s)
{
    int pad = script_int(s);
    script_cond(s, pad >= 0 && pad < 2 && PadGetTarget(PADS + pad * PAD_SIZE));
    return 0;
}

static int8_t __stdcall op_0E8D(Script *s)
{
    int pad = script_int(s);
    /* the secondary fire button is 4 in controller mode 1 (Ginput), else 7 */
    script_cond(s, pad >= 0 && pad < 2 &&
                   (GetPadState(s, pad, 17) || GetPadState(s, pad, *(uint8_t *)0x47F399 == 1 ? 4 : 7)));
    return 0;
}

static int8_t __stdcall op_0E6E(Script *s)
{
    if (!USE_MOUSE_3RD_PERSON)
        script_cond(s, AT(PADS, PAD_NEW_CROSS, int16_t) && !AT(PADS, PAD_OLD_CROSS, int16_t));
    else
        script_cond(s, KeyJustDown(CONTROLS, AT(CONTROLS, 0xB70 + 13 * 0x20, int32_t)));
    return 0;
}

static int8_t __stdcall op_0F13(Script *s)
{
    int pad = script_int(s);
    store_int(s, pad >= 0 && pad < 2 ? (int32_t)(GAME_TIME_MS - AT(PADS + pad * PAD_SIZE, PAD_LAST_TOUCHED, uint32_t)) : 0);
    return 0;
}

/* ---- weapons --------------------------------------------------------- */

static int8_t __stdcall op_0E84(Script *s)
{
    void *wi;
    int id, skill;
    script_collect(s, 2);
    id = ScriptParams[0].i;
    skill = ScriptParams[1].i;
    if (id < 0 || id >= 70 || skill < 0 || skill > 3)
        wi = NULL;
    else
        wi = GetWeaponInfo(id, id >= 22 && id <= 32 ? skill : 1);
    store_ptr(s, wi);
    script_cond(s, wi != NULL);
    return 0;
}

static uint8_t *wi_arg(Script *s) { return (uint8_t *)(uintptr_t)script_int(s); }

static int8_t __stdcall op_0E85(Script *s)
{
    uint8_t *wi = wi_arg(s);
    int32_t v[2] = { wi ? AT(wi, WI_MODEL1, int32_t) : -1, wi ? AT(wi, WI_MODEL2, int32_t) : -1 };
    store_ints(s, v, 2);
    return 0;
}

static int8_t wi_field(Script *s, int off) { uint8_t *wi = wi_arg(s); store_int(s, wi ? AT(wi, off, int32_t) : 0); return 0; }

static int8_t __stdcall op_0E86(Script *s) { return wi_field(s, WI_FLAGS); }
static int8_t __stdcall op_0E87(Script *s) { return wi_field(s, WI_ANIM_GROUP); }
static int8_t __stdcall op_0E89(Script *s) { return wi_field(s, WI_FIRE); }
static int8_t __stdcall op_0E8A(Script *s) { return wi_field(s, WI_SLOT); }

static int8_t __stdcall op_0E88(Script *s)
{
    uint8_t *wi = wi_arg(s);
    store_int(s, wi ? AT(wi, WI_CLIP, int16_t) : 0);
    return 0;
}

/* ---- models ---------------------------------------------------------- */

static int8_t __stdcall op_0E7F(Script *s)
{
    uint8_t *mi = model_info(script_int(s));
    store_int(s, mi ? (*(ModelTypeFn *)(*(uint8_t **)mi + MI_VT_MODEL_TYPE))(mi) : -1);
    return 0;
}

static int8_t __stdcall op_0EF8(Script *s)
{
    uint8_t *mi = model_info(script_int(s));
    store_ptr(s, mi);
    script_cond(s, mi != NULL);
    return 0;
}

static int8_t __stdcall op_0EBA(Script *s)
{
    uint8_t *mi = model_info(script_int(s));
    int32_t v[2] = { mi ? AT(mi, PMI_PED_TYPE, int32_t) : -1, mi ? AT(mi, PMI_STAT, int32_t) : -1 };
    store_ints(s, v, 2);
    script_cond(s, mi != NULL);
    return 0;
}

static int8_t __stdcall op_0E98(Script *s)
{
    int model = script_int(s);
    RequestModel(model, 2 | 4 | 0x10);    /* keep in memory, mission required, priority */
    AddToResources(RESOURCE_MANAGER, model, 2, s);
    return 0;
}

static int8_t __stdcall op_0E9C(Script *s)
{
    char name[128];
    int id = -1;
    void *mi;
    script_string(s, name, sizeof name);
    mi = GetModelByName(name, &id);
    store_int(s, id);
    script_cond(s, mi && id > -1);
    return 0;
}

static int8_t __stdcall op_0E9D(Script *s)
{
    char name[128];
    uint32_t off, size;
    script_string(s, name, sizeof name);
    script_cond(s, EXTRA_OBJECTS_DIR && DirFindItem(EXTRA_OBJECTS_DIR, name, &off, &size));
    return 0;
}

/* A free id in [start, end]: no model in the image, and not loaded. */
static int8_t __stdcall op_0E9E(Script *s)
{
    int id, end, found = 0;
    script_collect(s, 2);
    end = ScriptParams[1].i;
    for (id = ScriptParams[0].i; id <= end && id >= 0 && id < model_count(); id++) {
        if (!IsObjectInCdImage(id) && STREAMING_INFO[id * SI_SIZE + SI_LOAD_STATE] != 1) {
            found = 1;
            break;
        }
    }
    store_int(s, found ? id : -1);
    script_cond(s, found);
    return 0;
}

static int8_t __stdcall op_0E9F(Script *s) { (void)s; RemoveAllUnused(); return 0; }

static int8_t __stdcall op_0EA3(Script *s)
{
    int id = script_int(s);
    uint8_t *mi = model_info(id);
    if (mi && AT(mi, MI_REFCOUNT, uint16_t) == 0)
        RemoveModel(id);
    return 0;
}

/* ---- the camera ------------------------------------------------------ */

static int8_t __stdcall op_0E64(Script *s) { store_int(s, AT(active_cam(), CCAM_MODE, int16_t)); return 0; }

static int8_t __stdcall op_0EB2(Script *s)
{
    float in[3], out[3] = { 0, 0, 0 };
    uint8_t *m = AT(THE_CAMERA, ENT_MATRIX, uint8_t *);
    script_collect(s, 3);
    in[0] = ScriptParams[0].f;
    in[1] = ScriptParams[1].f;
    in[2] = ScriptParams[2].f;
    if (m)
        RwV3dTransformPoint(out, in, m);
    store_floats(s, out, 3);
    return 0;
}

static int8_t __stdcall op_0F0E(Script *s)
{
    float out[6] = { 0 };
    script_collect(s, 4);
    Find3rdPersonTarget(THE_CAMERA, ScriptParams[0].f, ScriptParams[1].f, ScriptParams[2].f, ScriptParams[3].f,
                        out, out + 3);
    store_floats(s, out, 6);
    return 0;
}

static int8_t __stdcall op_0F0F(Script *s)
{
    float f[2] = { AT(THE_CAMERA, CAM_LOD_MULT, float), AT(THE_CAMERA, CAM_GEN_MULT, float) };
    store_floats(s, f, 2);
    return 0;
}

static float wrap360(float a)
{
    while (a < 0)
        a += 360.0f;
    return a;
}

static int8_t __stdcall op_0F10(Script *s)
{
    float out[3] = { 0, 0, 0 };
    float *m = AT(THE_CAMERA, ENT_MATRIX, float *);
    if (m) {
        out[0] = wrap360(GetATanOfXY(m[2], fsqrt(m[0] * m[0] + m[1] * m[1])) * -57.295776f - 270.0f);
        out[2] = wrap360(GetATanOfXY(m[0], m[1]) * -57.295776f - 180.0f);
        out[1] = wrap360(GetATanOfXY(m[6], fsqrt(m[4] * m[4] + m[5] * m[5])) * -57.295776f - 270.0f);
    }
    store_floats(s, out, 3);
    return 0;
}

static int8_t __stdcall op_0F11(Script *s)
{
    float f[2] = { AT(THE_CAMERA, CAM_WATER_DIST, float), AT(THE_CAMERA, CAM_WATER_HEIGHT, float) };
    store_floats(s, f, 2);
    return 0;
}

static int8_t __stdcall op_0F12(Script *s)
{
    int32_t v[2] = { (int32_t)(uintptr_t)THE_CAMERA, (int32_t)(uintptr_t)active_cam() };
    store_ints(s, v, 2);
    return 0;
}

static int8_t __stdcall op_0F14(Script *s)
{
    uint8_t *c = active_cam();
    float f[2] = { AT(c, CCAM_HANGLE, float) * -57.295776f, AT(c, CCAM_VANGLE, float) * 57.295776f };
    store_floats(s, f, 2);
    return 0;
}

static int8_t __stdcall op_0F15(Script *s)
{
    uint8_t *c = active_cam();
    script_collect(s, 2);
    AT(c, CCAM_HANGLE, float) = ScriptParams[0].f * -0.01745329252f;
    AT(c, CCAM_VANGLE, float) = ScriptParams[1].f * 0.01745329252f;
    return 0;
}

/* ---- entities by pointer and math ------------------------------------ */

static int8_t __stdcall op_0EEE(Script *s)
{
    static const float zero[3];
    uint8_t *e = (uint8_t *)(uintptr_t)script_int(s);
    store_floats(s, e ? entity_pos(e) : zero, 3);
    return 0;
}

static int8_t __stdcall op_0EEF(Script *s)
{
    uint8_t *e = (uint8_t *)(uintptr_t)script_int(s);
    store_float(s, e ? GetHeading(e) : 0);
    return 0;
}

static int8_t __stdcall op_0EE1(Script *s) { store_floats(s, (float *)newopcodes_colpoint((uint32_t)script_int(s)), 3); return 0; }

static void normalise(float *v)
{
    float l = fsqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0) {
        v[0] /= l;
        v[1] /= l;
        v[2] /= l;
    }
}

static void cross(float *out, const float *a, const float *b)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

/* A quaternion facing a direction, from a basis on it and world up, as
 * CLEO+ builds it. q is x, y, z, w. */
static int8_t __stdcall op_0EB3(Script *s)
{
    float f[3], up[3] = { 0, 0, 1 }, r[3], u[3], d, t, *q;
    script_collect(s, 4);
    q = newopcodes_quat((uint32_t)ScriptParams[0].i);
    f[0] = ScriptParams[1].f;
    f[1] = ScriptParams[2].f;
    f[2] = ScriptParams[3].f;
    normalise(f);
    if (f[0] == 0) f[0] = 0.001f;
    if (f[1] == 0) f[1] = 0.001f;
    if (f[2] == 0) f[2] = 0.001f;
    d = up[0] * f[0] + up[1] * f[1] + up[2] * f[2];
    up[0] -= f[0] * d;
    up[1] -= f[1] * d;
    up[2] -= f[2] * d;
    normalise(up);
    normalise(f);
    cross(r, up, f);
    cross(u, f, r);
    t = r[0] + u[1] + f[2];
    if (t > 0) {
        float n = fsqrt(t + 1.0f);
        q[3] = n * 0.5f;
        n = 0.5f / n;
        q[0] = (u[2] - f[1]) * n;
        q[1] = (f[0] - r[2]) * n;
        q[2] = (r[1] - u[0]) * n;
    } else if (r[0] >= u[1] && r[0] >= f[2]) {
        float n = fsqrt(1.0f + r[0] - u[1] - f[2]), k = 0.5f / n;
        q[0] = 0.5f * n;
        q[1] = (r[1] + u[0]) * k;
        q[2] = (r[2] + f[0]) * k;
        q[3] = (u[2] - f[1]) * k;
    } else if (u[1] > f[2]) {
        float n = fsqrt(1.0f + u[1] - r[0] - f[2]), k = 0.5f / n;
        q[0] = (u[0] + r[1]) * k;
        q[1] = 0.5f * n;
        q[2] = (f[1] + u[2]) * k;
        q[3] = (f[0] - r[2]) * k;
    } else {
        float n = fsqrt(1.0f + f[2] - r[0] - u[1]), k = 0.5f / n;
        q[0] = (f[0] + r[2]) * k;
        q[1] = (f[1] + u[2]) * k;
        q[2] = 0.5f * n;
        q[3] = (r[1] - u[0]) * k;
    }
    QuatNormalise(q);
    return 0;
}

static int8_t __stdcall op_0F0D(Script *s)
{
    float *m, origin[3], dir[3];
    script_collect(s, 7);
    m = newopcodes_matrix((uint32_t)ScriptParams[0].i);
    origin[0] = ScriptParams[1].f;
    origin[1] = ScriptParams[2].f;
    origin[2] = ScriptParams[3].f;
    dir[0] = ScriptParams[4].f;
    dir[1] = ScriptParams[5].f;
    dir[2] = ScriptParams[6].f;
    CreateMatFromVec(FX_SYSTEM, m, origin, dir);
    return 0;
}

/* MSVC's rand(), so a seed gives the numbers CLEO+ gives. */
static uint32_t lcg;
static int msvc_rand(void)
{
    lcg = lcg * 214013 + 2531011;
    return (lcg >> 16) & 0x7FFF;
}

static int8_t __stdcall op_0EBC(Script *s)
{
    int min, max;
    script_collect(s, 3);
    lcg = (uint32_t)ScriptParams[0].i;
    min = ScriptParams[1].i;
    max = ScriptParams[2].i - 1;
    store_int(s, max >= min ? min + (int)((uint32_t)msvc_rand() % (uint32_t)(max - min + 1)) : min);
    return 0;
}

static int8_t __stdcall op_0EBD(Script *s)
{
    float min, max;
    script_collect(s, 3);
    lcg = (uint32_t)ScriptParams[0].i;
    min = ScriptParams[1].f;
    max = ScriptParams[2].f;
    store_float(s, (float)msvc_rand() / 32767.0f * (max - min) + min);
    return 0;
}

/* ---- pad and camera locks -------------------------------------------- */

#define PAD_DISABLE          0x10E     /* uint16: 1 movement, 2 controls */

static uint8_t pad_lock[2], pad_locked[2];      /* bit 1 movement, 2 controls */

/* The camera's mouse and stick reads: each operand points at one of three
 * floats; a lock points them at zero. CLEO+ lists 0x50FB10, the start of
 * an fmul; its operand is 0x50FB12. */
static const uint32_t cam_sites[] = {
    0x510C28, 0x50FB28, 0x50F048, 0x511E0A, 0x52228E, 0x510C1C, 0x50F03C, 0x50FB18, 0x511DFE, 0x5222A0,
    0x523F71, 0x524069, 0x525705, 0x525715, 0x50EF84, 0x50F032, 0x50FB12, 0x50FB7A, 0x510B5D, 0x510C12,
    0x510C83, 0x511D46, 0x511DF4, 0x5199EB, 0x522265, 0x522329, 0x523DE1, 0x523F69, 0x524061, 0x52546C,
    0x5255CC, 0x5256F9, 0x5263B8,
};
static uint32_t cam_saved[sizeof cam_sites / sizeof cam_sites[0]];
static int cam_lock, cam_locked;
static const float cam_zero;

static void cam_apply(void)
{
    uint32_t zero = (uint32_t)(uintptr_t)&cam_zero;
    size_t i;
    for (i = 0; i < sizeof cam_sites / sizeof cam_sites[0]; i++) {
        uint32_t *at = (uint32_t *)(uintptr_t)cam_sites[i], v = *at;
        if (cam_lock) {
            if (v == 0xB6EC18 || v == 0xB6EC1C || v == 0x85A5C4) {
                cam_saved[i] = v;
                ms_patch(at, &zero, 4);
            }
        } else if (v == zero && cam_saved[i]) {
            ms_patch(at, &cam_saved[i], 4);
        }
    }
    cam_locked = cam_lock;
}

static int8_t __stdcall op_0E60(Script *s) { cam_lock = script_int(s) == 0; cam_apply(); return 0; }

static int8_t set_pad_lock(Script *s, int bit)
{
    int pad, on;
    script_collect(s, 2);
    pad = ScriptParams[0].i;
    on = ScriptParams[1].i;
    if (pad >= 0 && pad < 2) {
        if (on)
            pad_lock[pad] &= ~bit;
        else
            pad_lock[pad] |= bit;
    }
    return 0;
}

static int8_t __stdcall op_0E68(Script *s) { return set_pad_lock(s, 2); }
static int8_t __stdcall op_0E69(Script *s) { return set_pad_lock(s, 1); }

static void locks_tick(void)
{
    int pad, bit;
    for (pad = 0; pad < 2; pad++)
        for (bit = 1; bit <= 2; bit <<= 1) {
            uint16_t *d = (uint16_t *)(PADS + pad * PAD_SIZE + PAD_DISABLE);
            if (pad_lock[pad] & bit) {
                *d |= bit;
                pad_locked[pad] |= bit;
            } else if (pad_locked[pad] & bit) {
                *d &= ~bit;
                pad_locked[pad] &= ~bit;
            }
        }
    /* again each frame: other mods (GTA V Hud) put the reads back */
    if (cam_lock || cam_locked)
        cam_apply();
}

/* ---- save slot, restarts, model names --------------------------------- */

#define MENU_SAVE_SLOT       0x15F     /* CMenuManager::m_bSelectedSaveGame */

static int save_slot = -2, restarts;
static char *model_names;
static uint32_t old_loaded, old_restart, old_new_game, old_save, old_model_by_name;

/* Points a call at fn and returns what it called, or 0 when the site is
 * not a call. Another mod's redirect is chained, not dropped. */
static uint32_t redirect_call(uint32_t site, void *fn)
{
    uint8_t *at = (uint8_t *)(uintptr_t)site, code[5];
    uint32_t old;
    if (at[0] != 0xE8)
        return 0;
    old = site + 5 + *(int32_t *)(at + 1);
    code[0] = 0xE8;
    *(int32_t *)(code + 1) = (int32_t)((uint8_t *)fn - (at + 5));
    return ms_patch(at, code, 5) ? old : 0;
}

static void __cdecl on_loaded(void)
{
    ((VoidFn)(uintptr_t)old_loaded)();
    save_slot = MENU_MANAGER[MENU_SAVE_SLOT];
}

static void __cdecl on_restart(void)
{
    if (save_slot > -2)
        restarts++;
    save_slot = -1;
    pad_lock[0] = pad_lock[1] = 0;
    cam_lock = 0;
    ((VoidFn)(uintptr_t)old_restart)();
}

static void __cdecl on_new_game(void)
{
    ((VoidFn)(uintptr_t)old_new_game)();
    save_slot = -1;
}

static void __cdecl on_save(void)
{
    save_slot = MENU_MANAGER[MENU_SAVE_SLOT];
    ((VoidFn)(uintptr_t)old_save)();
}

/* CFileLoader's call while reading the IDE files: keeps each model's name. */
static void *__cdecl on_model_by_name(char *name, int *index)
{
    void *mi = ((ModelByNameFn)(uintptr_t)old_model_by_name)(name, index);
    if (mi && model_names && *index >= 0 && *index < model_count() && name[0])
        copy_string(model_names + *index * 32, name, 32);
    return mi;
}

static int8_t __stdcall op_0E2C(Script *s) { store_int(s, save_slot + 1); return 0; }
static int8_t __stdcall op_0E2D(Script *s) { script_cond(s, restarts == 0); return 0; }

static int8_t __stdcall op_0F17(Script *s)
{
    int id = script_int(s);
    store_ptr(s, model_names && id >= 0 && id < model_count() ? model_names + id * 32 : NULL);
    return 0;
}

static void hooks_install(void)
{
    old_loaded = redirect_call(0x5D19CE, on_loaded);
    old_restart = redirect_call(0x53C6DB, on_restart);
    old_new_game = redirect_call(0x748E1C, on_new_game);
    old_save = redirect_call(0x618F51, on_save);
    model_names = VirtualAlloc(NULL, (size_t)model_count() * 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (model_names && !(old_model_by_name = redirect_call(0x5B624E, on_model_by_name)))
        ms_log("cleoplus: the model loader's call at 005B624E is not there; model names stay empty");
}

/* ---- last-frame damage, damage anims -------------------------------- */

/* Kept per pool slot, keyed by the handle (its low byte changes when the
 * slot is reused). Damage counts for the frame it was taken and the next,
 * wherever MoonLoader runs its scripts in the frame. */
#define DAMAGE_SLOTS 2048

typedef struct { int32_t ref; void *damager; int32_t weapon, part; float intensity; uint32_t frame; } PedDamage;
typedef struct { int32_t ref; void *ped; int32_t type; float intensity; uint32_t frame; } CarDamage;
static PedDamage *ped_damage;
static CarDamage *car_damage;
static int32_t ignore_anims[DAMAGE_SLOTS];      /* ped handles */

static int slot_of(uint8_t *e, uint32_t pool_ptr, int size)
{
    uint8_t *pool = *(uint8_t **)(uintptr_t)pool_ptr;
    int i;
    if (!pool || !e)
        return -1;
    i = (int)((e - AT(pool, 0, uint8_t *)) / size);
    return i >= 0 && i < AT(pool, 8, int32_t) && i < DAMAGE_SLOTS ? i : -1;
}

static void __cdecl record_ped_damage(uint8_t *calc, uint8_t *ped)
{
    int i = slot_of(ped, 0xB74490, 0x7C4);
    if (i < 0 || !ped_damage)
        return;
    ped_damage[i].ref = GetPedRef(ped);
    ped_damage[i].damager = AT(calc, 0, void *);
    ped_damage[i].intensity = AT(calc, 4, float);
    ped_damage[i].part = AT(calc, 8, int32_t);
    ped_damage[i].weapon = AT(calc, 12, int32_t);
    ped_damage[i].frame = FRAME_COUNTER;
}

static void __cdecl record_car_damage(uint8_t *veh, void *ped, int32_t type, uint32_t intensity)
{
    int i;
    if (!(intensity & 0x7FFFFFFF) || (i = slot_of(veh, 0xB74494, 0xA18)) < 0 || !car_damage)
        return;
    car_damage[i].ref = GetVehRef(veh);
    car_damage[i].ped = ped;
    car_damage[i].type = type;
    memcpy(&car_damage[i].intensity, &intensity, 4);
    car_damage[i].frame = FRAME_COUNTER;
}

static uint32_t anim_next;
static uint8_t *anim_resume;

/* Where CPed's damage anim code goes on: its epilogue for a ped told to
 * ignore them. */
static uint32_t __cdecl choose_anims(uint8_t *ped)
{
    int i = slot_of(ped, 0xB74490, 0x7C4);
    return i >= 0 && ignore_anims[i] && ignore_anims[i] == GetPedRef(ped) ? 0x4B4019 : (uint32_t)(uintptr_t)anim_resume;
}

static void damage_install(void);

static int recent(uint32_t frame) { return frame && FRAME_COUNTER - frame <= 1; }

static int8_t __stdcall op_0EB5(Script *s)
{
    int32_t ref = script_int(s);
    damage_install();
    uint8_t *p = GetPedAt(ref);
    int i = slot_of(p, 0xB74490, 0x7C4);
    PedDamage *d = i >= 0 && ped_damage && ped_damage[i].ref == ref && recent(ped_damage[i].frame) ? &ped_damage[i] : NULL;
    ScriptParams[0].p = d ? d->damager : NULL;
    ScriptParams[1].i = d ? d->weapon : 0;
    ScriptParams[2].i = d ? d->part : 0;
    ScriptParams[3].f = d ? d->intensity : 0;
    script_store(s, 4);
    script_cond(s, d && d->intensity > 0);
    return 0;
}

static int8_t __stdcall op_0EB6(Script *s)
{
    int32_t ref = script_int(s);
    damage_install();
    uint8_t *v = GetVehAt(ref);
    int i = slot_of(v, 0xB74494, 0xA18);
    CarDamage *d = i >= 0 && car_damage && car_damage[i].ref == ref && recent(car_damage[i].frame) ? &car_damage[i] : NULL;
    ScriptParams[0].i = d && d->ped && slot_of(d->ped, 0xB74490, 0x7C4) >= 0 ? GetPedRef(d->ped) : -1;
    ScriptParams[1].i = d ? d->type : 0;
    ScriptParams[2].f = d ? d->intensity : 0;
    script_store(s, 3);
    script_cond(s, d && d->intensity != 0);
    return 0;
}

static int8_t __stdcall op_0ED9(Script *s)
{
    int32_t ref;
    int i;
    script_collect(s, 2);
    damage_install();
    ref = ScriptParams[0].i;
    i = slot_of(GetPedAt(ref), 0xB74490, 0x7C4);
    if (i >= 0)
        ignore_anims[i] = ScriptParams[1].i == 1 ? ref : 0;
    return 0;
}

static uint8_t *emit_call(uint8_t *t, void *fn)
{
    *t++ = 0xE8;
    *(int32_t *)t = (int32_t)((uint8_t *)fn - (t + 4));
    return t + 4;
}

static uint8_t *emit_jmp(uint8_t *t, uint32_t to)
{
    *t++ = 0xE9;
    *(int32_t *)t = (int32_t)((uint8_t *)(uintptr_t)to - (t + 4));
    return t + 4;
}

/* A jmp at site to code, nop-padded to len bytes. */
static int jump_from(uint32_t site, int len, uint8_t *code)
{
    uint8_t j[8] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
    *(int32_t *)(j + 1) = (int32_t)(code - (uint8_t *)(uintptr_t)(site + 5));
    return ms_patch((void *)(uintptr_t)site, j, len);
}

/* On first use: nothing is hooked for scripts that never ask. */
static void damage_install(void)
{
    static int done;
    static const uint8_t ped_site[7] = { 0xC7, 0x45, 0x04, 0, 0, 0, 0 };
    static const uint8_t car_site[6] = { 0xD9, 0x86, 0xC0, 0x04, 0, 0 };
    static const uint8_t anim_site[6] = { 0x8B, 0x8B, 0x34, 0x05, 0, 0 };
    uint8_t *code, *t;
    if (done)
        return;
    done = 1;
    code = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    ped_damage = VirtualAlloc(NULL, DAMAGE_SLOTS * sizeof *ped_damage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    car_damage = VirtualAlloc(NULL, DAMAGE_SLOTS * sizeof *car_damage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!code)
        return;
    t = code;
    if (memcmp((void *)0x4B5AF9, ped_site, 7) == 0) {          /* CPedDamageResponseCalculator: esi it, edi the ped */
        uint8_t *at = t;
        *t++ = 0x60; *t++ = 0x9C;                               /* pushad, pushfd */
        *t++ = 0x57; *t++ = 0x56;                               /* push edi, push esi */
        t = emit_call(t, record_ped_damage);
        *t++ = 0x83; *t++ = 0xC4; *t++ = 0x08;                  /* add esp, 8 */
        *t++ = 0x9D; *t++ = 0x61;                               /* popfd, popad */
        memcpy(t, ped_site, 7); t += 7;
        t = emit_jmp(t, 0x4B5B00);
        jump_from(0x4B5AF9, 7, at);
    } else {
        ms_log("cleoplus: 004B5AF9 is patched by something else; no char damage");
    }
    if (memcmp((void *)0x6D804F, car_site, 6) == 0) {          /* esi the car, edi the ped, ebx the weapon, [esp+90h] the damage */
        uint8_t *at = t;
        *t++ = 0x60; *t++ = 0x9C;
        *t++ = 0xFF; *t++ = 0xB4; *t++ = 0x24; *(int32_t *)t = 0x90 + 36; t += 4;  /* push [esp+0B4h] */
        *t++ = 0x53; *t++ = 0x57; *t++ = 0x56;                  /* push ebx, edi, esi */
        t = emit_call(t, record_car_damage);
        *t++ = 0x83; *t++ = 0xC4; *t++ = 0x10;
        *t++ = 0x9D; *t++ = 0x61;
        memcpy(t, car_site, 6); t += 6;
        t = emit_jmp(t, 0x6D8055);
        jump_from(0x6D804F, 6, at);
    } else {
        ms_log("cleoplus: 006D804F is patched by something else; no car damage");
    }
    if (memcmp((void *)0x4B4029, anim_site, 6) == 0) {         /* ebx the ped; the flags of cmp eax, 1Bh are live */
        uint8_t *at = t;
        anim_resume = t + 64;
        *t++ = 0x60; *t++ = 0x9C;
        *t++ = 0x53;                                            /* push ebx */
        t = emit_call(t, choose_anims);
        *t++ = 0x83; *t++ = 0xC4; *t++ = 0x04;
        *t++ = 0xA3; *(uint32_t **)t = &anim_next; t += 4;      /* mov [anim_next], eax */
        *t++ = 0x9D; *t++ = 0x61;
        *t++ = 0xFF; *t++ = 0x25; *(uint32_t **)t = &anim_next; t += 4;  /* jmp [anim_next] */
        t = anim_resume;
        memcpy(t, anim_site, 6); t += 6;
        t = emit_jmp(t, 0x4B402F);
        jump_from(0x4B4029, 6, at);
    } else {
        ms_log("cleoplus: 004B4029 is patched by something else; setCharIgnoreDamageAnims does nothing");
    }
}

/* ---------------------------------------------------------------------- */

void cleoplus_tick(void)
{
    static Script probe;
    int pad, id;
    for (pad = 0; pad < 2; pad++)
        for (id = 0; id < 20; id++)
            buttons_last[pad][id] = GetPadState(&probe, pad, id);
    locks_tick();
}

void cleoplus_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0E00, op_0E00 }, { 0x0E01, op_0E01 }, { 0x0E02, op_0E02 }, { 0x0E04, op_0E04 },
        { 0x0E05, op_0E05 }, { 0x0E06, op_0E06 }, { 0x0E07, op_0E07 }, { 0x0E08, op_0E08 },
        { 0x0E09, op_0E09 }, { 0x0E0A, op_0E0A }, { 0x0E0B, op_0E0B }, { 0x0E0C, op_0E0C },
        { 0x0E0D, op_0E0D }, { 0x0E0F, op_0E0F }, { 0x0E1D, op_0E1D }, { 0x0E23, op_0E23 },
        { 0x0E24, op_0E24 }, { 0x0E25, op_0E25 }, { 0x0E26, op_0E26 }, { 0x0E2C, op_0E2C }, { 0x0E2D, op_0E2D }, { 0x0E32, op_0E32 },
        { 0x0E33, op_0E33 }, { 0x0E34, op_0E34 }, { 0x0E38, op_0E38 }, { 0x0E39, op_0E39 },
        { 0x0E3E, op_0E3E }, { 0x0E42, op_0E42 }, { 0x0E43, op_0E43 }, { 0x0E44, op_0E44 },
        { 0x0E46, op_0E46 }, { 0x0E47, op_0E47 }, { 0x0E48, op_0E48 }, { 0x0E49, op_0E49 },
        { 0x0E4A, op_0E4A }, { 0x0E4B, op_0E4B }, { 0x0E4C, op_0E4C }, { 0x0E59, op_0E59 },
        { 0x0E5A, op_0E5A }, { 0x0E5C, op_0E5C }, { 0x0E5D, op_0E5D }, { 0x0E5F, op_0E5F }, { 0x0E60, op_0E60 },
        { 0x0E61, op_0E61 }, { 0x0E64, op_0E64 }, { 0x0E65, op_0E65 }, { 0x0E66, op_0E66 },
        { 0x0E67, op_0E67 }, { 0x0E68, op_0E68 }, { 0x0E69, op_0E69 }, { 0x0E6C, op_0E6C }, { 0x0E6D, op_0E6D }, { 0x0E6E, op_0E6E },
        { 0x0E71, op_0E71 }, { 0x0E7F, op_0E7F }, { 0x0E82, op_0E82 }, { 0x0E83, op_0E83 },
        { 0x0E84, op_0E84 }, { 0x0E85, op_0E85 }, { 0x0E86, op_0E86 }, { 0x0E87, op_0E87 },
        { 0x0E88, op_0E88 }, { 0x0E89, op_0E89 }, { 0x0E8A, op_0E8A }, { 0x0E8B, op_0E8B },
        { 0x0E8C, op_0E8C }, { 0x0E8D, op_0E8D }, { 0x0E8E, op_0E8E }, { 0x0E8F, op_0E8F },
        { 0x0E90, op_0E90 }, { 0x0E91, op_0E91 }, { 0x0E92, op_0E92 }, { 0x0E93, op_0E93 },
        { 0x0E94, op_0E94 }, { 0x0E95, op_0E95 }, { 0x0E96, op_0E96 }, { 0x0E97, op_0E97 },
        { 0x0E98, op_0E98 }, { 0x0E9C, op_0E9C }, { 0x0E9D, op_0E9D }, { 0x0E9E, op_0E9E },
        { 0x0E9F, op_0E9F }, { 0x0EA3, op_0EA3 }, { 0x0EA4, op_0EA4 }, { 0x0EA5, op_0EA5 },
        { 0x0EA6, op_0EA6 }, { 0x0EAA, op_0EAA }, { 0x0EAB, op_0EAB }, { 0x0EAC, op_0EAC },
        { 0x0EAD, op_0EAD }, { 0x0EAE, op_0EAE }, { 0x0EAF, op_0EAF }, { 0x0EB0, op_0EB0 },
        { 0x0EB1, op_0EB1 }, { 0x0EB2, op_0EB2 }, { 0x0EB3, op_0EB3 }, { 0x0EB4, op_0EB4 }, { 0x0EB5, op_0EB5 }, { 0x0EB6, op_0EB6 },
        { 0x0EB7, op_0EB7 }, { 0x0EB8, op_0EB8 }, { 0x0EB9, op_0EB9 }, { 0x0EBA, op_0EBA },
        { 0x0EBB, op_0EBB }, { 0x0EBC, op_0EBC }, { 0x0EBD, op_0EBD }, { 0x0EC7, op_0EC7 },
        { 0x0EC8, op_0EC8 }, { 0x0EC9, op_0EC9 }, { 0x0ECA, op_0ECA }, { 0x0ECB, op_0ECB },
        { 0x0ECC, op_0ECC }, { 0x0ECD, op_0ECD }, { 0x0ECE, op_0ECE }, { 0x0ECF, op_0ECF }, { 0x0ED9, op_0ED9 },
        { 0x0EE1, op_0EE1 }, { 0x0EEE, op_0EEE }, { 0x0EEF, op_0EEF }, { 0x0EF5, op_0EF5 }, { 0x0EF6, op_0EF6 },
        { 0x0EF8, op_0EF8 }, { 0x0EF9, op_0EF9 }, { 0x0EFA, op_0EFA }, { 0x0EFB, op_0EFB },
        { 0x0EFC, op_0EFC }, { 0x0EFD, op_0EFD }, { 0x0EFF, op_0EFF }, { 0x0F0D, op_0F0D },
        { 0x0F0E, op_0F0E }, { 0x0F0F, op_0F0F }, { 0x0F10, op_0F10 }, { 0x0F11, op_0F11 },
        { 0x0F12, op_0F12 }, { 0x0F13, op_0F13 }, { 0x0F14, op_0F14 }, { 0x0F15, op_0F15 },
        { 0x0F16, op_0F16 }, { 0x0F17, op_0F17 },
    };
    size_t i;
    hooks_install();
    /* With CLEO they stay moonsamp_run's: CLEO+.cleo keeps its own. */
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        if (script_cleo_present())
            script_keep(ops[i].op, ops[i].fn);
        else
            script_register(ops[i].op, ops[i].fn);
    }
}
