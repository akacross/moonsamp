/* NewOpcodes' drawing opcodes: 2D shapes, sprites and text, queued by the
 * scripts and drawn once a frame from Render2dStuff (its empty call at
 * 0x53E293, as plugin-sdk's drawing event), spotlights from the effects pass
 * (0x53E1E0), and coronas, shadows and lights straight into the game's own
 * lists. Textures come from TXD, PNG, BMP or DDS files. Both hooks go in with
 * the first queued item. */
#include "moonsamp.h"

#define MAX_SHAPES     100
#define MAX_VERTS      20
#define MAX_SPRITES    200
#define MAX_PRINTS     100
#define MAX_SPOTLIGHTS 100
#define MAX_TEXTURES   500
#define MAX_TXDS       100

#define RW_ENGINE      (*(uint8_t **)0xC97B24)
#define RS_GLOBAL      ((uint8_t *)0xC17040)     /* width at +4, height at +8 */
#define SHADOW_TEXTURES ((void **)0xC403E0)
#define CORONA_TEXTURES ((void **)0xC3E000)
#define TEXTURE_NAME   0x10

enum { RS_RASTER = 1, RS_SRCBLEND = 10, RS_DESTBLEND = 11, RS_VERTEXALPHA = 12 };

typedef struct {
    float x, y, z, rhw;
    uint32_t color;
    float u, v;
} Vertex;

typedef struct { uint8_t r, g, b, a; } Rgba;

typedef int (__cdecl *StateSetFn)(int, void *);
typedef int (__cdecl *Im2DFn)(int, Vertex *, int);
typedef void (__cdecl *AnyRectFn)(float, float, float, float, float, float, float, float,
                                  const Rgba *, const Rgba *, const Rgba *, const Rgba *);
typedef void (__cdecl *VoidFn)(void);
typedef void (__cdecl *FontFFn)(float);
typedef void (__cdecl *FontIFn)(int);
typedef void (__cdecl *FontScaleFn)(float, float);
typedef void (__cdecl *FontColorFn)(Rgba);
typedef void (__cdecl *FontBackFn)(int, int);
typedef void (__cdecl *PrintFn)(float, float, const char *);
typedef void (__cdecl *ShadowFn)(int, void *, const float *, float, float, float, float, int, int, int, int,
                                 float, int, float, void *, int);
typedef void (__cdecl *PermShadowFn)(int, void *, const float *, float, float, float, float, int, int, int, int,
                                     float, uint32_t, float);
typedef void (__cdecl *LightFn)(int, float, float, float, float, float, float, float, float, float, float,
                                int, int, void *);
typedef void (__cdecl *CoronaFn)(uint32_t, void *, int, int, int, int, const float *, float, float, void *,
                                 int, int, int, int, float, int, float, int, float, int, int);
typedef void (__cdecl *ConeFn)(int, float, float, float, float, float, float, float, float, int, int,
                               float *, float *, float *, int, float, float, float, float);
typedef void *(__cdecl *ImageReadFn)(const char *);
typedef void *(__cdecl *ImageFn)(void *);
typedef void *(__cdecl *ApplyMaskFn)(void *, const void *);
typedef void *(__cdecl *FindFormatFn)(void *, int, int *, int *, int *, int *);
typedef void *(__cdecl *RasterCreateFn)(int, int, int, int);
typedef void *(__cdecl *RasterSetFn)(void *, void *);
typedef void *(__cdecl *DdsReadFn)(const char *, const char *);
typedef void *(__cdecl *FindTexFn)(void *, const char *);
typedef int (__cdecl *TxdNameFn)(const char *);
typedef int (__cdecl *TxdLoadFn)(int, const char *);
typedef void *(__cdecl *TxdGetFn)(int);
typedef void (__cdecl *TxdIdFn)(int);

#define DrawAnyRect          ((AnyRectFn)0x727CC0)
#define Font_SetScale        ((FontScaleFn)0x719380)
#define Font_SetColor        ((FontColorFn)0x719430)
#define Font_SetFontStyle    ((FontIFn)0x719490)
#define Font_SetWrapx        ((FontFFn)0x7194D0)
#define Font_SetCentreSize   ((FontFFn)0x7194E0)
#define Font_SetRightWrap    ((FontFFn)0x7194F0)
#define Font_SetDropColor    ((FontColorFn)0x719510)
#define Font_SetDropShadow   ((FontIFn)0x719570)
#define Font_SetEdge         ((FontIFn)0x719590)
#define Font_SetProportional ((FontIFn)0x7195B0)
#define Font_SetBackground   ((FontBackFn)0x7195C0)
#define Font_SetBackColor    ((FontColorFn)0x7195E0)
#define Font_SetJustify      ((FontIFn)0x719600)
#define Font_SetOrientation  ((FontIFn)0x719610)
#define Font_PrintString     ((PrintFn)0x71A700)
#define StoreShadow          ((ShadowFn)0x707390)
#define AddPermanentShadow   ((PermShadowFn)0x706F60)
#define AddLight             ((LightFn)0x7000E0)
#define RegisterCorona       ((CoronaFn)0x6FC180)
#define PreSearchLightCone   ((VoidFn)0x6C4650)
#define SearchLightCone      ((ConeFn)0x6C58E0)
#define PostSearchLightCone  ((VoidFn)0x6C46E0)
#define RtPNGImageRead       ((ImageReadFn)0x7CF9B0)
#define RtBMPImageRead       ((ImageReadFn)0x7CDF60)
#define RwImageDestroy       ((ImageFn)0x802740)
#define RwImageMakeMask      ((ImageFn)0x802A20)
#define RwImageApplyMask     ((ApplyMaskFn)0x802AF0)
#define RwImageFindRasterFormat ((FindFormatFn)0x8042C0)
#define RwRasterCreate       ((RasterCreateFn)0x7FB230)
#define RwRasterSetFromImage ((RasterSetFn)0x804290)
#define RwTextureCreate      ((ImageFn)0x7F37C0)
#define RwTextureDestroy     ((ImageFn)0x7F3820)
#define RwD3D9DDSTextureRead ((DdsReadFn)0x820A10)
#define RwTexDictionaryFindNamedTexture ((FindTexFn)0x7F39F0)
#define FindTxdSlot          ((TxdNameFn)0x731850)
#define AddTxdSlot           ((TxdNameFn)0x731C80)
#define LoadTxd              ((TxdLoadFn)0x7320B0)
#define GetTxd               ((TxdGetFn)0x408340)
#define TxdAddRef            ((TxdIdFn)0x731A00)
#define RemoveTxdSlot        ((TxdIdFn)0x731CD0)

static struct {
    uint8_t prim, count, alpha, src, dst;
    void *texture;
    Vertex v[MAX_VERTS];
} shapes[MAX_SHAPES];
static struct {
    void *texture;
    float left, top, right, bottom, angle;
    Rgba color[4];
} sprites[MAX_SPRITES];
static struct {
    char text[128];
    float x, y, w, h, wrap;
    uint8_t style, prop, align, justify, outline, shadow, background;
    Rgba color, drop, back;
} prints[MAX_PRINTS];
static struct {
    float from[3], to[3], base, target;
    uint8_t shadow, intensity, flag1, flag2;
} spots[MAX_SPOTLIGHTS];
static Vertex scratch_verts[MAX_VERTS];
static void *textures[MAX_TEXTURES];
static struct { char name[32]; int id; } txds[MAX_TXDS];
static unsigned num_shapes, num_sprites, num_prints, num_spots;
static int drew_2d, drew_spots;
static uint32_t corona_id = 10000;

static float fsin(float x) { float r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static float fcos(float x) { float r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }

static void state(int which, uintptr_t value)
{
    ((StateSetFn)AT(RW_ENGINE, 0x20, void *))(which, (void *)value);
}

static void raster(void *texture) { state(RS_RASTER, texture ? (uintptr_t)AT(texture, 0, void *) : 0); }

static void store_ptr(Script *s, const void *p)
{
    ScriptParams[0].p = (void *)p;
    script_store(s, 1);
}

static Rgba rgba(Script *s)
{
    Rgba c;
    script_collect(s, 4);
    c.r = (uint8_t)ScriptParams[0].i, c.g = (uint8_t)ScriptParams[1].i;
    c.b = (uint8_t)ScriptParams[2].i, c.a = (uint8_t)ScriptParams[3].i;
    return c;
}

static void draw_frame(void)
{
    unsigned i, k;
    drew_2d = 1;
    for (i = 0; i < num_shapes; i++) {
        raster(shapes[i].texture);
        state(RS_VERTEXALPHA, shapes[i].alpha);
        state(RS_SRCBLEND, shapes[i].src);
        state(RS_DESTBLEND, shapes[i].dst);
        ((Im2DFn)AT(RW_ENGINE, 0x30, void *))(shapes[i].prim, shapes[i].v, shapes[i].count);
    }
    if (num_shapes) {
        raster(NULL);
        state(RS_VERTEXALPHA, 1);
        state(RS_SRCBLEND, 5);
        state(RS_DESTBLEND, 6);
    }
    num_shapes = 0;
    for (i = 0; i < num_prints; i++) {
        Font_SetScale(prints[i].w, prints[i].h);
        Font_SetFontStyle(prints[i].style);
        Font_SetProportional(prints[i].prop != 0);
        Font_SetJustify(prints[i].justify);
        Font_SetOrientation(prints[i].align);
        if (prints[i].outline || prints[i].shadow) {
            if (prints[i].outline)
                Font_SetEdge(prints[i].outline);
            else
                Font_SetDropShadow(prints[i].shadow);
            Font_SetDropColor(prints[i].drop);
        } else {
            Font_SetEdge(0);
            Font_SetDropShadow(0);
        }
        Font_SetBackground(prints[i].background != 0, 0);
        if (prints[i].background)
            Font_SetBackColor(prints[i].back);
        Font_SetColor(prints[i].color);
        if (prints[i].align == 1)
            Font_SetWrapx(prints[i].wrap);
        else if (prints[i].align == 0)
            Font_SetCentreSize(prints[i].wrap);
        else if (prints[i].align == 2)
            Font_SetRightWrap(prints[i].wrap);
        Font_PrintString(prints[i].x, prints[i].y, prints[i].text);
    }
    num_prints = 0;
    for (i = 0; i < num_sprites; i++) {
        float x[4] = { sprites[i].left, sprites[i].right, sprites[i].left, sprites[i].right };
        float y[4] = { sprites[i].bottom, sprites[i].bottom, sprites[i].top, sprites[i].top };
        raster(sprites[i].texture);
        if (sprites[i].angle != 0.0f) {
            float cx = (sprites[i].left + sprites[i].right) / 2.0f, cy = (sprites[i].top + sprites[i].bottom) / 2.0f;
            float c = fcos(sprites[i].angle), sn = fsin(sprites[i].angle);
            for (k = 0; k < 4; k++) {
                float dx = x[k] - cx, dy = y[k] - cy;
                x[k] = cx + dx * c + dy * sn;
                y[k] = cy - dx * sn + dy * c;
            }
        }
        DrawAnyRect(x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3],
                    &sprites[i].color[0], &sprites[i].color[1], &sprites[i].color[2], &sprites[i].color[3]);
    }
    if (num_sprites)
        raster(NULL);
    num_sprites = 0;
}

static void draw_spotlights(void)
{
    static float spare[MAX_SPOTLIGHTS][3][3];
    unsigned i;
    drew_spots = 1;
    if (!num_spots)
        return;
    PreSearchLightCone();
    for (i = 0; i < num_spots; i++)
        SearchLightCone(15000 + i, spots[i].from[0], spots[i].from[1], spots[i].from[2],
                        spots[i].to[0], spots[i].to[1], spots[i].to[2], spots[i].target,
                        spots[i].intensity / 255.0f, spots[i].flag1, spots[i].shadow,
                        spare[i][0], spare[i][1], spare[i][2], spots[i].flag2, spots[i].base, 0.0f, 0.0f, 1.0f);
    PostSearchLightCone();
    num_spots = 0;
}

/* A stub that keeps every register for the original call: pushad, pushfd,
 * call fn, popfd, popad, jmp to the original. */
static int hook_call(uint8_t *site, void (*fn)(void), const char *what)
{
    uint8_t call[5] = { 0xE8 }, *t, *to;
    if (site[0] != 0xE8) {
        ms_log("draw: %s at %08lX is %02X, not a call; it is off", what, (DWORD)site, site[0]);
        return 0;
    }
    to = site + 5 + *(int32_t *)(site + 1);
    t = VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t)
        return 0;
    t[0] = 0x60, t[1] = 0x9C, t[2] = 0xE8;
    *(int32_t *)(t + 3) = (int32_t)((uint8_t *)(void *)fn - (t + 7));
    t[7] = 0x9D, t[8] = 0x61, t[9] = 0xE9;
    *(int32_t *)(t + 10) = (int32_t)(to - (t + 14));
    *(int32_t *)(call + 1) = (int32_t)(t - (site + 5));
    return ms_patch(site, call, 5);
}

/* 1 for the 2D queue, 2 for spotlights */
static int hooked(int which)
{
    static int tried, ok;
    if (!tried) {
        tried = 1;
        ok = hook_call((uint8_t *)0x53E293, draw_frame, "the 2D call") |
             hook_call((uint8_t *)0x53E1E0, draw_spotlights, "the effects call") << 1;
        ms_log("draw: 2D drawing %s, spotlights %s", ok & 1 ? "hooked" : "off", ok & 2 ? "hooked" : "off");
    }
    return (ok & which) != 0;
}

/* Once a frame, before the scripts. Idle skips both passes while the menu
 * is up or the screen fades in: what was queued for a frame that drew
 * nothing goes, so no queue outlives its textures. */
void draw_tick(void)
{
    if (!drew_2d)
        num_shapes = num_sprites = num_prints = 0;
    if (!drew_spots)
        num_spots = 0;
    drew_2d = drew_spots = 0;
    corona_id = 10000;
}

/* 0D40 draw_2d_shape_type %3d% texture %4d% numVerts %2d% pVerts %1d%
 * vertexAlpha %5d% srcBlend %6d% dstBlend %7d% _unused %8d%: vertices of 1
 * are the scratch ones */
static Vertex *verts(int32_t p) { return p == 1 ? scratch_verts : (Vertex *)(uintptr_t)p; }

static int8_t __stdcall op_0D40(Script *s)
{
    unsigned i, n;
    Vertex *v;
    script_collect(s, 8);
    n = (unsigned)ScriptParams[2].i;
    v = verts(ScriptParams[3].i);
    if (num_shapes >= MAX_SHAPES || !v || !hooked(1)) {
        script_cond(s, 0);
        return 0;
    }
    if (n > MAX_VERTS)
        n = MAX_VERTS;
    shapes[num_shapes].prim = (uint8_t)ScriptParams[0].i;
    shapes[num_shapes].texture = ScriptParams[1].p;
    shapes[num_shapes].count = (uint8_t)n;
    shapes[num_shapes].alpha = (uint8_t)ScriptParams[4].i;
    shapes[num_shapes].src = (uint8_t)ScriptParams[5].i;
    shapes[num_shapes].dst = (uint8_t)ScriptParams[6].i;
    for (i = 0; i < n; i++)
        shapes[num_shapes].v[i] = v[i];
    num_shapes++;
    script_cond(s, 1);
    return 0;
}

/* 0D41 set_vertices %1d% vertex %2d% xyz %5d% %6d% %7d% rhw %8d% RGBA %9d%
 * %10d% %11d% %12d% uv %13d% %14d% invertX %3d% invertY %4d% */
static int8_t __stdcall op_0D41(Script *s)
{
    Vertex *v;
    int k;
    script_collect(s, 14);
    v = verts(ScriptParams[0].i);
    k = ScriptParams[1].i;
    if (!v || k < 1 || (v == scratch_verts && k > MAX_VERTS))
        return 0;
    v += k - 1;
    v->x = ScriptParams[2].f, v->y = ScriptParams[3].f, v->z = ScriptParams[4].f, v->rhw = ScriptParams[5].f;
    v->color = (uint32_t)(ScriptParams[9].i & 0xFF) << 24 | (uint32_t)(ScriptParams[6].i & 0xFF) << 16 |
               (uint32_t)(ScriptParams[7].i & 0xFF) << 8 | (uint32_t)(ScriptParams[8].i & 0xFF);
    v->u = ScriptParams[10].f, v->v = ScriptParams[11].f;
    if (ScriptParams[12].i) {
        v->x = (float)AT(RS_GLOBAL, 4, int32_t) - v->x;
        v->u = 1.0f - v->u;
    }
    if (ScriptParams[13].i) {
        v->y = (float)AT(RS_GLOBAL, 8, int32_t) - v->y;
        v->v = 1.0f - v->v;
    }
    return 0;
}

/* 0D45 rotate_2d_vertices_shape %1d% num_verts %2d% aroundXY %3d% %4d% angle %5d% */
static int8_t __stdcall op_0D45(Script *s)
{
    Vertex *v;
    int n;
    float cx, cy, c, sn;
    script_collect(s, 5);
    v = verts(ScriptParams[0].i);
    n = ScriptParams[1].i;
    cx = ScriptParams[2].f, cy = ScriptParams[3].f;
    c = fcos(ScriptParams[4].f * 0.0174532925f), sn = fsin(ScriptParams[4].f * 0.0174532925f);
    if (v == scratch_verts && n > MAX_VERTS)
        n = MAX_VERTS;
    for (; v && n > 0; n--, v++) {
        float dx = v->x - cx, dy = v->y - cy;
        v->x = cx + dx * c + dy * sn;
        v->y = cy - dx * sn + dy * c;
    }
    return 0;
}

/* 0D42 load_txd %1s% from %2s%: once per name */
static int8_t __stdcall op_0D42(Script *s)
{
    char name[32], path[260];
    int i, free_ = -1, id;
    script_string(s, name, sizeof name);
    script_string(s, path, sizeof path);
    for (i = 0; i < MAX_TXDS; i++) {
        if (!txds[i].name[0]) {
            if (free_ < 0)
                free_ = i;
        } else if (!lstrcmpiA(txds[i].name, name)) {
            script_cond(s, 1);
            return 0;
        }
    }
    if (free_ < 0 || !name[0] || (id = AddTxdSlot(name)) < 0) {
        script_cond(s, 0);
        return 0;
    }
    if (!LoadTxd(id, path)) {
        RemoveTxdSlot(id);
        script_cond(s, 0);
        return 0;
    }
    TxdAddRef(id);
    lstrcpyA(txds[free_].name, name);
    txds[free_].id = id;
    script_cond(s, 1);
    return 0;
}

static int8_t find_texture(Script *s, const char *tex, int slot)
{
    void *dict = slot >= 0 ? GetTxd(slot) : NULL;
    void *t = dict ? RwTexDictionaryFindNamedTexture(dict, tex) : NULL;
    store_ptr(s, t);
    script_cond(s, t != NULL);
    return 0;
}

/* 0D44 %3d% = find_texture %1s% in_dictionary %2s%, 0D46 ... in_dictionary %2d% */
static int8_t __stdcall op_0D44(Script *s)
{
    char tex[32], txd[32];
    script_string(s, tex, sizeof tex);
    script_string(s, txd, sizeof txd);
    return find_texture(s, tex, FindTxdSlot(txd));
}

static int8_t __stdcall op_0D46(Script *s)
{
    char tex[32];
    script_string(s, tex, sizeof tex);
    return find_texture(s, tex, script_int(s));
}

/* 0D50 draw_shadow %1d% at %2d% %3d% %4d% angle %7d% length %8d% ...: the
 * shadow textures 1-10 are the game's, else a texture */
static void *shadow_texture(uint32_t t)
{
    static const uint8_t slot[11] = { 5, 0, 1, 5, 2, 6, 8, 7, 3, 4, 11 };   /* in 0xC403E0's order */
    return t < 11 ? SHADOW_TEXTURES[slot[t]] : (void *)(uintptr_t)t;
}

static void shadow_axes(float w, float h, float deg, float *a)
{
    float r = deg * 0.0174532925f;
    a[0] = fcos(r) * w / 2.0f, a[1] = fsin(r) * w / 2.0f;
    a[2] = -fsin(r) * h / 2.0f, a[3] = fcos(r) * h / 2.0f;
}

static int8_t __stdcall op_0D50(Script *s)
{
    float a[4];
    script_collect(s, 14);
    shadow_axes(ScriptParams[4].f, ScriptParams[5].f, ScriptParams[6].f, a);
    StoreShadow(ScriptParams[0].i, shadow_texture(ScriptParams[8].u), &ScriptParams[1].f, a[0], a[1], a[2], a[3],
                (int16_t)ScriptParams[9].i, (uint8_t)ScriptParams[10].i, (uint8_t)ScriptParams[11].i,
                (uint8_t)ScriptParams[12].i, ScriptParams[7].f, 0, 1.0f, ScriptParams[13].p, 0);
    return 0;
}

static int8_t __stdcall op_0D51(Script *s)
{
    float a[4];
    script_collect(s, 14);
    shadow_axes(ScriptParams[4].f, ScriptParams[5].f, ScriptParams[6].f, a);
    AddPermanentShadow(ScriptParams[0].i, shadow_texture(ScriptParams[8].u), &ScriptParams[1].f, a[0], a[1], a[2], a[3],
                       (int16_t)ScriptParams[9].i, (uint8_t)ScriptParams[10].i, (uint8_t)ScriptParams[11].i,
                       (uint8_t)ScriptParams[12].i, ScriptParams[7].f, ScriptParams[13].u, 1.0f);
    return 0;
}

/* 0D52 draw_light %1d% at %2d% %3d% %4d% direction %5d% %6d% %7d% radius %8d%
 * rgb %9d% %10d% %11d% entity %12d% */
static int8_t __stdcall op_0D52(Script *s)
{
    script_collect(s, 12);
    AddLight(ScriptParams[0].i, ScriptParams[1].f, ScriptParams[2].f, ScriptParams[3].f,
             ScriptParams[4].f, ScriptParams[5].f, ScriptParams[6].f, ScriptParams[7].f,
             (uint8_t)ScriptParams[8].i / 255.0f, (uint8_t)ScriptParams[9].i / 255.0f, (uint8_t)ScriptParams[10].i / 255.0f,
             0, 0, ScriptParams[11].p);
    return 0;
}

/* 0D53 draw_corona ..., 0D54 draw_corona_ex ...: textures 0-9 are the game's
 * corona textures, ids from 10000 and the same each frame */
static void *corona_texture(uint32_t t)
{
    if (t >= 10)
        return (void *)(uintptr_t)t;
    return CORONA_TEXTURES[t >= 1 && (t <= 4 || t == 9) ? t : 0];
}

static int8_t __stdcall op_0D53(Script *s)
{
    script_collect(s, 10);
    RegisterCorona(corona_id++, ScriptParams[5].p, (uint8_t)ScriptParams[1].i, (uint8_t)ScriptParams[2].i,
                   (uint8_t)ScriptParams[3].i, (uint8_t)ScriptParams[4].i, &ScriptParams[6].f, ScriptParams[9].f,
                   100.0f, corona_texture(ScriptParams[0].u), 0, 0, 0, 0, 0.0f, 0, 0.15f, 0, 15.0f, 0, 0);
    return 0;
}

static int8_t __stdcall op_0D54(Script *s)
{
    script_collect(s, 18);
    RegisterCorona(corona_id++, ScriptParams[5].p, (uint8_t)ScriptParams[1].i, (uint8_t)ScriptParams[2].i,
                   (uint8_t)ScriptParams[3].i, (uint8_t)ScriptParams[4].i, &ScriptParams[6].f, ScriptParams[9].f,
                   ScriptParams[10].f, corona_texture(ScriptParams[0].u), (uint8_t)ScriptParams[12].i,
                   (uint8_t)ScriptParams[13].i, (uint8_t)ScriptParams[14].i, 0, 0.0f, 0, ScriptParams[11].f,
                   (uint8_t)ScriptParams[15].i, ScriptParams[16].f, (uint8_t)ScriptParams[17].i, 0);
    return 0;
}

/* 0D5B draw_spotlight from %1d% %2d% %3d% to %4d% %5d% %6d% base_radius %7d%
 * target_radius %8d% enable_shadow %9d% shadow_intensity %10d% flag1 %11d% flag2 %12d% */
static int8_t __stdcall op_0D5B(Script *s)
{
    int i;
    script_collect(s, 12);
    if (num_spots >= MAX_SPOTLIGHTS || !hooked(2))
        return 0;
    for (i = 0; i < 3; i++) {
        spots[num_spots].from[i] = ScriptParams[i].f;
        spots[num_spots].to[i] = ScriptParams[3 + i].f;
    }
    spots[num_spots].base = ScriptParams[6].f;
    spots[num_spots].target = ScriptParams[7].f;
    spots[num_spots].shadow = (uint8_t)ScriptParams[8].i;
    spots[num_spots].intensity = (uint8_t)ScriptParams[9].i;
    spots[num_spots].flag1 = (uint8_t)ScriptParams[10].i;
    spots[num_spots].flag2 = (uint8_t)ScriptParams[11].i;
    num_spots++;
    return 0;
}

/* Textures from files: kept until 0D7D unloads them. */
static void *keep_texture(void *t, const char *path)
{
    int i;
    const char *base = path, *p;
    char *name;
    if (!t)
        return NULL;
    for (i = 0; i < MAX_TEXTURES && textures[i]; i++)
        ;
    if (i == MAX_TEXTURES) {
        RwTextureDestroy(t);
        return NULL;
    }
    textures[i] = t;
    for (p = path; *p; p++)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    name = (char *)t + TEXTURE_NAME;
    for (i = 0; i < 31 && base[i] && base[i] != '.'; i++)
        name[i] = base[i];
    name[i] = 0;
    return t;
}

static void *texture_from_image(void *image)
{
    int w, h, depth, flags;
    void *r;
    RwImageFindRasterFormat(image, 4, &w, &h, &depth, &flags);
    r = RwRasterCreate(w, h, depth, flags);
    if (r)
        RwRasterSetFromImage(r, image);
    RwImageDestroy(image);
    return r ? RwTextureCreate(r) : NULL;
}

static int8_t loaded(Script *s, void *t)
{
    store_ptr(s, t);
    script_cond(s, t != NULL);
    return 0;
}

/* 0D61 %3d% = load_texture_from %1s% with_mask %2s% */
static int8_t __stdcall op_0D61(Script *s)
{
    char path[260], mask[260];
    void *image, *m;
    script_string(s, path, sizeof path);
    script_string(s, mask, sizeof mask);
    if (!(image = RtBMPImageRead(path)))
        return loaded(s, NULL);
    if (mask[0] && (m = RtBMPImageRead(mask))) {
        RwImageMakeMask(m);
        RwImageApplyMask(image, m);
        RwImageDestroy(m);
    }
    return loaded(s, keep_texture(texture_from_image(image), path));
}

/* 0D64 %2d% = load_texture_png_from %1s% */
static int8_t __stdcall op_0D64(Script *s)
{
    char path[260];
    void *image;
    script_string(s, path, sizeof path);
    image = RtPNGImageRead(path);
    return loaded(s, image ? keep_texture(texture_from_image(image), path) : NULL);
}

/* 0D7C %2d% = load_texture_dds_from %1s%: RW adds the .dds itself */
static int8_t __stdcall op_0D7C(Script *s)
{
    char path[260], name[260];
    int n;
    script_string(s, path, sizeof path);
    n = lstrlenA(path);
    if (n < 5)
        return loaded(s, NULL);
    lstrcpyA(name, path);
    name[n - 4] = 0;
    return loaded(s, keep_texture(RwD3D9DDSTextureRead(name, NULL), path));
}

/* 0D7D clean_loaded_texture %1d%: only ours are destroyed, but shapes and
 * sprites still queued with it go either way, so none binds a freed raster */
static int8_t __stdcall op_0D7D(Script *s)
{
    void *t = (void *)(uintptr_t)script_int(s);
    unsigned i, n;
    for (i = n = 0; t && i < num_shapes; i++)
        if (shapes[i].texture != t)
            shapes[n++] = shapes[i];
    if (t)
        num_shapes = n;
    for (i = n = 0; t && i < num_sprites; i++)
        if (sprites[i].texture != t)
            sprites[n++] = sprites[i];
    if (t)
        num_sprites = n;
    for (i = 0; t && i < MAX_TEXTURES; i++)
        if (textures[i] == t) {
            RwTextureDestroy(t);
            textures[i] = NULL;
            break;
        }
    return 0;
}

/* 0D65 print %1s% at %2d% %3d% scale %4d% %5d% style %6d%, 0D66 the same with
 * prop, align, wrap, justify, colour, outline, shadow and background */
static int queue_print(Script *s)
{
    if (num_prints >= MAX_PRINTS || !hooked(1))
        return -1;
    script_string(s, prints[num_prints].text, sizeof prints[0].text);
    script_collect(s, 5);
    prints[num_prints].x = ScriptParams[0].f, prints[num_prints].y = ScriptParams[1].f;
    prints[num_prints].w = ScriptParams[2].f, prints[num_prints].h = ScriptParams[3].f;
    prints[num_prints].style = (uint8_t)ScriptParams[4].i;
    return (int)num_prints;
}

static int8_t __stdcall op_0D65(Script *s)
{
    int i = queue_print(s);
    if (i < 0) {
        char skip[128];
        script_string(s, skip, sizeof skip);
        script_collect(s, 5);
        return 0;
    }
    prints[i].prop = 1, prints[i].align = 1, prints[i].wrap = 600.0f, prints[i].justify = 0;
    prints[i].color = (Rgba){ 255, 255, 255, 255 };
    prints[i].outline = 1, prints[i].shadow = 0, prints[i].background = 0;
    prints[i].drop = (Rgba){ 0, 0, 0, 255 };
    prints[i].back = (Rgba){ 0, 0, 0, 0 };
    num_prints++;
    return 0;
}

static int8_t __stdcall op_0D66(Script *s)
{
    int i = queue_print(s);
    if (i < 0) {
        char skip[128];
        script_string(s, skip, sizeof skip);
        script_collect(s, 24);
        return 0;
    }
    script_collect(s, 4);
    prints[i].prop = (uint8_t)ScriptParams[0].i, prints[i].align = (uint8_t)ScriptParams[1].i;
    prints[i].wrap = ScriptParams[2].f, prints[i].justify = (uint8_t)ScriptParams[3].i;
    prints[i].color = rgba(s);
    script_collect(s, 2);
    prints[i].outline = (uint8_t)ScriptParams[0].i, prints[i].shadow = (uint8_t)ScriptParams[1].i;
    prints[i].drop = rgba(s);
    prints[i].background = (uint8_t)script_int(s);
    prints[i].back = rgba(s);
    num_prints++;
    return 0;
}

/* 0D7E draw_sprite_with_texture %1d% at_cornerA %2d% %3d% cornerB %4d% %5d%
 * color %6d% %7d% %8d% %9d% angle %10d%, 0D7F with four colours: corners in
 * the 640x448 screen */
static int8_t queue_sprite(Script *s, int colors)
{
    float w = (float)AT(RS_GLOBAL, 4, int32_t), h = (float)AT(RS_GLOBAL, 8, int32_t);
    int i, k = (int)num_sprites;
    script_collect(s, 5);
    if (num_sprites < MAX_SPRITES && hooked(1)) {
        sprites[k].texture = ScriptParams[0].p;
        sprites[k].left = ScriptParams[1].f * w / 640.0f, sprites[k].top = ScriptParams[2].f * h / 448.0f;
        sprites[k].right = ScriptParams[3].f * w / 640.0f, sprites[k].bottom = ScriptParams[4].f * h / 448.0f;
    } else
        k = -1;
    for (i = 0; i < colors; i++) {
        Rgba c = rgba(s);
        if (k >= 0)
            sprites[k].color[i] = c;
    }
    for (; i < 4; i++)
        if (k >= 0)
            sprites[k].color[i] = sprites[k].color[0];
    if (k >= 0) {
        sprites[k].angle = script_float(s) * 0.0174532925f;
        num_sprites++;
    } else
        script_float(s);
    return 0;
}

static int8_t __stdcall op_0D7E(Script *s) { return queue_sprite(s, 1); }
static int8_t __stdcall op_0D7F(Script *s) { return queue_sprite(s, 4); }

void draw_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0D40, op_0D40 }, { 0x0D41, op_0D41 }, { 0x0D42, op_0D42 }, { 0x0D44, op_0D44 },
        { 0x0D45, op_0D45 }, { 0x0D46, op_0D46 }, { 0x0D50, op_0D50 }, { 0x0D51, op_0D51 },
        { 0x0D52, op_0D52 }, { 0x0D53, op_0D53 }, { 0x0D54, op_0D54 }, { 0x0D5B, op_0D5B },
        { 0x0D61, op_0D61 }, { 0x0D64, op_0D64 }, { 0x0D65, op_0D65 }, { 0x0D66, op_0D66 },
        { 0x0D7C, op_0D7C }, { 0x0D7D, op_0D7D }, { 0x0D7E, op_0D7E }, { 0x0D7F, op_0D7F },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        if (script_cleo_present())
            script_keep(ops[i].op, ops[i].fn);
        else
            script_register(ops[i].op, ops[i].fn);
    }
}
