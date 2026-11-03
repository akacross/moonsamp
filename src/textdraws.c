/* Textdraws. CTextDrawPool and CTextDraw are laid out the same in R3-1 and
 * 0.3.DL (every field offset below occurs equally often in both). */
#include "moonsamp.h"

#define TD_COUNT        2304
#define TD_OBJECTS      0x2400      /* after BOOL m_bNotEmpty[2304] */
#define TD_TEXT         0x000       /* char[801], as set by the server */
#define TD_LETTER_W     0x963
#define TD_LETTER_H     0x967
#define TD_LETTER_COLOR 0x96B
#define TD_BOX          0x971
#define TD_BOX_W        0x972
#define TD_BOX_H        0x976
#define TD_BOX_COLOR    0x97A
#define TD_X            0x98B
#define TD_Y            0x98F

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
    store_fields(s, arg_textdraw(s), offs, 3);
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
        AT(td, TD_LETTER_COLOR, uint32_t) = ScriptParams[3].u;
    }
    return 0;
}

/* 0C53 samp textdraw %1d% get_box_enabled %2d% color %3d% size %4d% %5d% */
static int8_t __stdcall op_0C53(Script *s)
{
    uint8_t *td = arg_textdraw(s);
    ScriptParams[0].i = td ? AT(td, TD_BOX, uint8_t) : 0;
    ScriptParams[1].i = td ? AT(td, TD_BOX_COLOR, int32_t) : 0;
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
        AT(td, TD_BOX_COLOR, uint32_t) = ScriptParams[2].u;
        AT(td, TD_BOX_W, float) = ScriptParams[3].f;
        AT(td, TD_BOX_H, float) = ScriptParams[4].f;
    }
    return 0;
}

void textdraws_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0C5D, op_0C5D }, { 0x0C5A, op_0C5A }, { 0x0C50, op_0C50 }, { 0x0C5B, op_0C5B },
        { 0x0C51, op_0C51 }, { 0x0C5C, op_0C5C }, { 0x0C52, op_0C52 }, { 0x0C53, op_0C53 },
        { 0x0C49, op_0C49 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
