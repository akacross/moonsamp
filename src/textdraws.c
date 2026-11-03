/* Textdraws and 3D texts. CTextDrawPool, CTextDraw and CLabelPool are laid out
 * the same in R3-1 and 0.3.DL (every offset below occurs equally often in
 * both). */
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

void textdraws_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0C5D, op_0C5D }, { 0x0C5A, op_0C5A }, { 0x0C50, op_0C50 }, { 0x0C5B, op_0C5B },
        { 0x0C51, op_0C51 }, { 0x0C5C, op_0C5C }, { 0x0C52, op_0C52 }, { 0x0C53, op_0C53 },
        { 0x0C49, op_0C49 }, { 0x0C54, op_0C54 }, { 0x0C55, op_0C55 }, { 0x0C56, op_0C56 },
        { 0x0C57, op_0C57 }, { 0x0C58, op_0C58 }, { 0x0C59, op_0C59 }, { 0x0C46, op_0C46 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
