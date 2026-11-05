/* Textdraws and 3D texts. CTextDrawPool, CTextDraw and CLabelPool are laid out
 * the same in every supported build. */
#include "moonsamp.h"

#define TD_COUNT        2304
#define TD_OBJECTS      0x2400      /* after BOOL m_bNotEmpty[2304] */
#define TD_TEXT         0x000       /* char[801], as set by the server */
#define TD_LETTER_W     0x963
#define TD_LETTER_H     0x967
#define TD_LETTER_COLOR 0x96B     /* colours are ABGR here, ARGB to scripts */
#define TD_CENTER       0x970
#define TD_BOX          0x971
#define TD_BOX_W        0x972
#define TD_BOX_H        0x976
#define TD_BOX_COLOR    0x97A
#define TD_PROPORTIONAL 0x97E
#define TD_BACK_COLOR   0x97F     /* shadow and outline colour */
#define TD_SHADOW       0x983
#define TD_OUTLINE      0x984
#define TD_LEFT         0x985
#define TD_RIGHT        0x986
#define TD_STYLE        0x987
#define TD_X            0x98B
#define TD_Y            0x98F
#define TD_MODEL        0x9A8
#define TD_ROTATION     0x9AA
#define TD_ZOOM         0x9B6
#define TD_VEH_COLORS   0x9BA

#define LABEL_COUNT     2048
#define LABEL_SIZE      0x1D      /* text, colour, x y z, distance, test LOS, player, vehicle */
#define LABEL_USED      0xE800

typedef void (THISCALL *SetTextFn)(void *, const char *);

static uint8_t *textdraw(int id)
{
    uint8_t *pool = samp_pool(POOL_TEXTDRAW);
    if (!pool || id < 0 || id >= TD_COUNT || !AT(pool, id * 4, int))
        return NULL;
    return AT(pool, TD_OBJECTS + id * 4, uint8_t *);
}

static uint8_t *arg_textdraw(Script *s) { return textdraw(script_int(s)); }

/* 0C5D samp textdraw %1d% is_exists */
static int8_t __stdcall op_0C5D(Script *s)
{
    script_cond(s, arg_textdraw(s) != NULL);
    return 0;
}

/* 0C5A samp textdraw %1d% get_string_to %2d%: into MoonLoader's buffer */
static int8_t __stdcall op_0C5A(Script *s)
{
    uint8_t *td = arg_textdraw(s);
    char *buf = (char *)(uintptr_t)script_int(s);
    if (buf)
        copy_string(buf, td ? (const char *)td + TD_TEXT : "", 801);
    return 0;
}

/* 0C50 samp textdraw %1d% set_string %2d% */
static int8_t __stdcall op_0C50(Script *s)
{
    static char buf[801];
    uint8_t *td = arg_textdraw(s);
    const Build *b = samp_build();
    copy_string(buf, script_string(s, buf, sizeof buf), sizeof buf);
    if (td && b)
        ((SetTextFn)(samp_base + b->textdraw_set_text))(td, buf);
    return 0;
}

static void store_fields(Script *s, uint8_t *td, const int *offs, int n)
{
    int i;
    for (i = 0; i < n; i++)
        ScriptParams[i].i = td ? AT(td, offs[i], int32_t) : 0;
    script_store(s, n);
}

/* 0C5B samp textdraw %1d% get_pos %2d% %3d% */
static int8_t __stdcall op_0C5B(Script *s)
{
    static const int offs[] = { TD_X, TD_Y };
    store_fields(s, arg_textdraw(s), offs, 2);
    return 0;
}

/* 0C51 samp textdraw %1d% set_pos %2d% %3d% */
static int8_t __stdcall op_0C51(Script *s)
{
    uint8_t *td;
    script_collect(s, 3);
    if ((td = textdraw(ScriptParams[0].i)) != NULL) {
        AT(td, TD_X, float) = ScriptParams[1].f;
        AT(td, TD_Y, float) = ScriptParams[2].f;
    }
    return 0;
}

/* 0C5C samp textdraw %1d% get_letter_size %2d% %3d% color %4d% */
static int8_t __stdcall op_0C5C(Script *s)
{
    static const int offs[] = { TD_LETTER_W, TD_LETTER_H, TD_LETTER_COLOR };
    uint8_t *td = arg_textdraw(s);
    int i;
    for (i = 0; i < 3; i++)
        ScriptParams[i].i = td ? AT(td, offs[i], int32_t) : 0;
    ScriptParams[2].u = abgr_argb(ScriptParams[2].u);
    script_store(s, 3);
    return 0;
}

/* 0C52 samp textdraw %1d% set_letter_size %2d% %3d% color %4d% */
static int8_t __stdcall op_0C52(Script *s)
{
    uint8_t *td;
    script_collect(s, 4);
    if ((td = textdraw(ScriptParams[0].i)) != NULL) {
        AT(td, TD_LETTER_W, float) = ScriptParams[1].f;
        AT(td, TD_LETTER_H, float) = ScriptParams[2].f;
        AT(td, TD_LETTER_COLOR, uint32_t) = abgr_argb(ScriptParams[3].u);
    }
    return 0;
}

/* 0C53 samp textdraw %1d% get_box_enabled %2d% color %3d% size %4d% %5d% */
static int8_t __stdcall op_0C53(Script *s)
{
    uint8_t *td = arg_textdraw(s);
    ScriptParams[0].i = td ? AT(td, TD_BOX, uint8_t) : 0;
    ScriptParams[1].u = td ? abgr_argb(AT(td, TD_BOX_COLOR, uint32_t)) : 0;
    ScriptParams[2].i = td ? AT(td, TD_BOX_W, int32_t) : 0;
    ScriptParams[3].i = td ? AT(td, TD_BOX_H, int32_t) : 0;
    script_store(s, 4);
    return 0;
}

/* 0C49 samp textdraw %1d% set_box %2d% color %3d% size %4d% %5d% */
static int8_t __stdcall op_0C49(Script *s)
{
    uint8_t *td;
    script_collect(s, 5);
    if ((td = textdraw(ScriptParams[0].i)) != NULL) {
        AT(td, TD_BOX, uint8_t) = ScriptParams[1].i != 0;
        AT(td, TD_BOX_COLOR, uint32_t) = abgr_argb(ScriptParams[2].u);
        AT(td, TD_BOX_W, float) = ScriptParams[3].f;
        AT(td, TD_BOX_H, float) = ScriptParams[4].f;
    }
    return 0;
}

static void store_one(Script *s, int32_t v)
{
    ScriptParams[0].i = v;
    script_store(s, 1);
}

/* 0C54 samp %2d% = textdraw %1d% get_align: 1 left, 2 centred, 3 right */
static int8_t __stdcall op_0C54(Script *s)
{
    uint8_t *td = arg_textdraw(s);
    int a = 0;
    if (td)
        a = AT(td, TD_CENTER, uint8_t) ? 2 : AT(td, TD_RIGHT, uint8_t) ? 3 : AT(td, TD_LEFT, uint8_t) ? 1 : 0;
    store_one(s, a);
    return 0;
}

/* 0C55 ... get_proportional, 0C56 ... get_style */
static int8_t __stdcall op_0C55(Script *s) { uint8_t *td = arg_textdraw(s); store_one(s, td ? AT(td, TD_PROPORTIONAL, uint8_t) : 0); return 0; }
static int8_t __stdcall op_0C56(Script *s) { uint8_t *td = arg_textdraw(s); store_one(s, td ? AT(td, TD_STYLE, int32_t) : 0); return 0; }

/* 0C57 samp textdraw %1d% get_shadow %2d% color %3d%, 0C58 ... get_outline */
static int8_t shadow_or_outline(Script *s, int off)
{
    uint8_t *td = arg_textdraw(s);
    ScriptParams[0].i = td ? AT(td, off, uint8_t) : 0;
    ScriptParams[1].u = td ? abgr_argb(AT(td, TD_BACK_COLOR, uint32_t)) : 0;
    script_store(s, 2);
    return 0;
}
static int8_t __stdcall op_0C57(Script *s) { return shadow_or_outline(s, TD_SHADOW); }
static int8_t __stdcall op_0C58(Script *s) { return shadow_or_outline(s, TD_OUTLINE); }

/* 0C59 samp textdraw %1d% get_model %2d% rotation %3d% %4d% %5d% zoom %6d% veh_color %7d% %8d% */
static int8_t __stdcall op_0C59(Script *s)
{
    uint8_t *td = arg_textdraw(s);
    ScriptParams[0].i = td ? AT(td, TD_MODEL, uint16_t) : 0;
    ScriptParams[1].f = td ? AT(td, TD_ROTATION, float) : 0;
    ScriptParams[2].f = td ? AT(td, TD_ROTATION + 4, float) : 0;
    ScriptParams[3].f = td ? AT(td, TD_ROTATION + 8, float) : 0;
    ScriptParams[4].f = td ? AT(td, TD_ZOOM, float) : 0;
    ScriptParams[5].i = td ? AT(td, TD_VEH_COLORS, uint16_t) : 0;
    ScriptParams[6].i = td ? AT(td, TD_VEH_COLORS + 2, uint16_t) : 0;
    script_store(s, 7);
    return 0;
}

/* 0C46 samp get_3d_text_info_by_id %1d% string_ptr %2d% color %3d% position %4d% %5d% %6d%
 * view_distance %7d% show_behind_walls %8d% attached_to_player %9d% attached_to_vehicle %10d% */
static int8_t __stdcall op_0C46(Script *s)
{
    static char empty[1];
    int id = script_int(s), i;
    uint8_t *pool = samp_pool(POOL_LABEL), *l = NULL;
    if (pool && id >= 0 && id < LABEL_COUNT && AT(pool, LABEL_USED + id * 4, int))
        l = pool + id * LABEL_SIZE;
    ScriptParams[0].p = l && AT(l, 0, char *) ? AT(l, 0, char *) : empty;
    ScriptParams[1].u = l ? AT(l, 4, uint32_t) : 0;
    for (i = 0; i < 4; i++)
        ScriptParams[2 + i].f = l ? AT(l, 8 + i * 4, float) : 0;
    ScriptParams[6].i = l ? !AT(l, 0x18, uint8_t) : 0;   /* SA-MP keeps "test line of sight" */
    ScriptParams[7].i = l ? AT(l, 0x19, uint16_t) : 0xFFFF;
    ScriptParams[8].i = l ? AT(l, 0x1B, uint16_t) : 0xFFFF;
    script_store(s, 9);
    return 0;
}

/* ---- creating and changing them (SAMPFUNCS's colours are ARGB) ---------- */

typedef struct {
    uint8_t flags;              /* box 1, left 2, right 4, centre 8, proportional 16 */
    float letter_w, letter_h;
    uint32_t letter_color;      /* ABGR, as the server sends it */
    float box_w, box_h;
    uint32_t box_color;
    uint8_t shadow, outline;
    uint32_t back_color;
    uint8_t style, unknown;
    float x, y;
    uint16_t model;
    float rotation[3], zoom;
    uint16_t colors[2];
} __attribute__((packed)) Transmit;

typedef void *(THISCALL *TdCreateFn)(void *, int, Transmit *, const char *);
typedef void (THISCALL *TdDeleteFn)(void *, int);
typedef void (THISCALL *LabelCreateFn)(void *, int, const char *, uint32_t, float, float, float, float, int, int, int);
typedef int (THISCALL *LabelDeleteFn)(void *, int);

/* 0C48 samp textdraw %1d% create %2d% pos %3d% %4d%: SA-MP's own defaults */
static int8_t __stdcall op_0C48(Script *s)
{
    static char text[801];
    Transmit t;
    int id = script_int(s);
    uint8_t *pool = samp_pool(POOL_TEXTDRAW);
    const Build *b = samp_build();
    copy_string(text, script_string(s, text, sizeof text), sizeof text);
    memset(&t, 0, sizeof t);
    t.x = script_float(s);
    t.y = script_float(s);
    t.flags = 16 | 2;
    t.letter_w = 0.48f; t.letter_h = 1.12f; t.letter_color = 0xFFE1E1E1;
    t.box_w = 1280.0f; t.box_h = 1280.0f; t.box_color = 0x80808080;
    t.shadow = 2; t.back_color = 0xFF000000; t.style = 1;
    t.zoom = 1.0f; t.colors[0] = t.colors[1] = 0xFFFF;
    if (pool && b && id >= 0 && id < TD_COUNT)
        ((TdCreateFn)(samp_base + b->td_create))(pool, id, &t, text);
    return 0;
}

/* 0C5E samp textdraw %1d% delete */
static int8_t __stdcall op_0C5E(Script *s)
{
    uint8_t *pool = samp_pool(POOL_TEXTDRAW);
    const Build *b = samp_build();
    int id = script_int(s);
    if (pool && b && textdraw(id))
        ((TdDeleteFn)(samp_base + b->td_delete))(pool, id);
    return 0;
}

/* 0C4A samp textdraw %1d% set_align %2d%: 1 left, 2 centred, 3 right */
static int8_t __stdcall op_0C4A(Script *s)
{
    uint8_t *td;
    script_collect(s, 2);
    if ((td = textdraw(ScriptParams[0].i)) != NULL) {
        AT(td, TD_LEFT, uint8_t) = ScriptParams[1].i == 1;
        AT(td, TD_CENTER, uint8_t) = ScriptParams[1].i == 2;
        AT(td, TD_RIGHT, uint8_t) = ScriptParams[1].i == 3;
    }
    return 0;
}

/* 0C4B ... set_proportional %2d%, 0C4C ... set_style %2d% */
static int8_t __stdcall op_0C4B(Script *s)
{
    uint8_t *td;
    script_collect(s, 2);
    if ((td = textdraw(ScriptParams[0].i)) != NULL)
        AT(td, TD_PROPORTIONAL, uint8_t) = ScriptParams[1].i != 0;
    return 0;
}

static int8_t __stdcall op_0C4C(Script *s)
{
    uint8_t *td;
    script_collect(s, 2);
    if ((td = textdraw(ScriptParams[0].i)) != NULL)
        AT(td, TD_STYLE, int32_t) = ScriptParams[1].i;
    return 0;
}

/* 0C4D ... set_shadow %2d% color %3d%, 0C4E ... set_outline %2d% color %3d% */
static int8_t set_shadow_or_outline(Script *s, int off)
{
    uint8_t *td;
    script_collect(s, 3);
    if ((td = textdraw(ScriptParams[0].i)) != NULL) {
        AT(td, off, uint8_t) = (uint8_t)ScriptParams[1].i;
        AT(td, TD_BACK_COLOR, uint32_t) = abgr_argb(ScriptParams[2].u);
    }
    return 0;
}
static int8_t __stdcall op_0C4D(Script *s) { return set_shadow_or_outline(s, TD_SHADOW); }
static int8_t __stdcall op_0C4E(Script *s) { return set_shadow_or_outline(s, TD_OUTLINE); }

/* 0C4F samp textdraw %1d% set_model %2d% rotation %3d% %4d% %5d% zoom %6d% veh_color %7d% %8d% */
static int8_t __stdcall op_0C4F(Script *s)
{
    uint8_t *td;
    script_collect(s, 8);
    if ((td = textdraw(ScriptParams[0].i)) != NULL) {
        AT(td, TD_MODEL, uint16_t) = (uint16_t)ScriptParams[1].i;
        AT(td, TD_ROTATION, float) = ScriptParams[2].f;
        AT(td, TD_ROTATION + 4, float) = ScriptParams[3].f;
        AT(td, TD_ROTATION + 8, float) = ScriptParams[4].f;
        AT(td, TD_ZOOM, float) = ScriptParams[5].f;
        AT(td, TD_VEH_COLORS, uint16_t) = (uint16_t)ScriptParams[6].i;
        AT(td, TD_VEH_COLORS + 2, uint16_t) = (uint16_t)ScriptParams[7].i;
    }
    return 0;
}

static uint8_t *label(int id)
{
    uint8_t *pool = samp_pool(POOL_LABEL);
    if (!pool || id < 0 || id >= LABEL_COUNT || !AT(pool, LABEL_USED + id * 4, int))
        return NULL;
    return pool + id * LABEL_SIZE;
}

/* CLabelPool::Create(id, text, RGBA, x, y, z, distance, test LOS, player, vehicle) */
static void label_create(int id, const char *text, uint32_t argb, const float *pos, float dist,
                         int behind_walls, int player, int vehicle)
{
    uint8_t *pool = samp_pool(POOL_LABEL);
    const Build *b = samp_build();
    if (pool && b && id >= 0 && id < LABEL_COUNT)
        ((LabelCreateFn)(samp_base + b->label_create))(pool, id, text, argb << 8 | argb >> 24,
                                                         pos[0], pos[1], pos[2], dist, !behind_walls,
                                                         player & 0xFFFF, vehicle & 0xFFFF);
}

/* the 3D text's parameters after its id: text, colour, position, distance,
 * behind walls, player, vehicle */
static int create_3d_text(Script *s, int id)
{
    static char text[2048];
    float pos[3], dist;
    uint32_t color;
    int walls, player, vehicle;
    copy_string(text, script_string(s, text, sizeof text), sizeof text);
    script_collect(s, 8);
    color = ScriptParams[0].u;
    pos[0] = ScriptParams[1].f; pos[1] = ScriptParams[2].f; pos[2] = ScriptParams[3].f;
    dist = ScriptParams[4].f;
    walls = ScriptParams[5].i != 0;
    player = ScriptParams[6].i;
    vehicle = ScriptParams[7].i;
    if (id < 0)                                 /* the highest free id */
        for (id = LABEL_COUNT - 1; id >= 0 && label(id); id--)
            ;
    if (id >= 0)
        label_create(id, text, color, pos, dist, walls, player, vehicle);
    return id;
}

/* 0B44 samp %10d% = create_3d_text %1d% color %2d% position %3d% %4d% %5d% view_distance %6d%
 * show_behind_walls %7d% attached_to_player %8d% attached_to_vehicle %9d% */
static int8_t __stdcall op_0B44(Script *s)
{
    int id = create_3d_text(s, -1);
    ScriptParams[0].i = id;
    script_store(s, 1);
    return 0;
}

/* 0C45 samp create_3d_text id %1d% text ... (as 0B44) */
static int8_t __stdcall op_0C45(Script *s)
{
    create_3d_text(s, script_int(s));
    return 0;
}

/* 0B45 samp destroy_3d_text %1d% */
static int8_t __stdcall op_0B45(Script *s)
{
    uint8_t *pool = samp_pool(POOL_LABEL);
    const Build *b = samp_build();
    int id = script_int(s);
    if (pool && b && label(id))
        ((LabelDeleteFn)(samp_base + b->label_delete))(pool, id);
    return 0;
}

/* 0C47 samp set_3d_text %1d% string %2d%: created again with the new text */
static int8_t __stdcall op_0C47(Script *s)
{
    static char text[2048];
    int id = script_int(s);
    uint8_t *l = label(id);
    copy_string(text, script_string(s, text, sizeof text), sizeof text);
    if (l) {
        uint32_t argb = AT(l, 4, uint32_t);
        float pos[3] = { AT(l, 8, float), AT(l, 0xC, float), AT(l, 0x10, float) };
        label_create(id, text, argb, pos, AT(l, 0x14, float), !AT(l, 0x18, uint8_t),
                     AT(l, 0x19, uint16_t), AT(l, 0x1B, uint16_t));
    }
    return 0;
}

void textdraws_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0C5D, op_0C5D }, { 0x0C5A, op_0C5A }, { 0x0C50, op_0C50 }, { 0x0C5B, op_0C5B },
        { 0x0C51, op_0C51 }, { 0x0C5C, op_0C5C }, { 0x0C52, op_0C52 }, { 0x0C53, op_0C53 },
        { 0x0C49, op_0C49 }, { 0x0C54, op_0C54 }, { 0x0C55, op_0C55 }, { 0x0C56, op_0C56 },
        { 0x0C57, op_0C57 }, { 0x0C58, op_0C58 }, { 0x0C59, op_0C59 }, { 0x0C46, op_0C46 },
        { 0x0C48, op_0C48 }, { 0x0C5E, op_0C5E }, { 0x0C4A, op_0C4A }, { 0x0C4B, op_0C4B },
        { 0x0C4C, op_0C4C }, { 0x0C4D, op_0C4D }, { 0x0C4E, op_0C4E }, { 0x0C4F, op_0C4F },
        { 0x0B44, op_0B44 }, { 0x0C45, op_0C45 }, { 0x0B45, op_0B45 }, { 0x0C47, op_0C47 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
