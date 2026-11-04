/* SA-MP's dialog box: show, read, change, close, respond.
 *
 * A dialog a script shows (sampShowDialog) is a client-side one: its
 * response must not reach the server, and sampHasDialogRespond reports it.
 * moonsamp shows it as a server-side dialog with m_bServerside = 2. SA-MP
 * tests that field for non-zero, so it builds and sends the DialogResponse
 * RPC as usual. The outgoing RPC hook sees the 2, keeps the response for
 * sampHasDialogRespond and drops the RPC. */
#include "moonsamp.h"

#define DLG_LISTBOX    0x20
#define DLG_EDITBOX    0x24
#define DLG_ACTIVE     0x28
#define DLG_TYPE       0x2C
#define DLG_ID         0x30
#define DLG_SERVERSIDE 0x81
#define CLIENT_SIDE    2
#define RPC_DIALOG_RESPONSE 62

typedef void (THISCALL *ShowFn)(void *, int, int, const char *, const char *, const char *, const char *, int);
typedef void (THISCALL *CloseFn)(void *, char);
typedef int (THISCALL *SelectedFn)(void *, int);
typedef void (THISCALL *SelectFn)(void *, int);
typedef const char *(THISCALL *GetTextFn)(void *);
typedef void (THISCALL *SetTextFn)(void *, const char *, int);

static struct {
    int pending, id, button, list;
    char text[256];
} response;
static int sending;     /* 0BC9's own RPC goes to the server */

static void *dialog(void)
{
    const Build *b = samp_build();
    return b ? samp_global(b->dialog) : NULL;
}

static void *fn(uint32_t rva) { return (void *)(samp_base + rva); }

/* From the outgoing RPC hook: DialogResponse is
 * u16 dialog, u8 button, u16 list item, u8 length, text. True to drop it. */
int dialog_response_sent(const uint8_t *d, int bits)
{
    void *dlg = dialog();
    int len;
    if (sending || !dlg || AT(dlg, DLG_SERVERSIDE, int) != CLIENT_SIDE || bits < 6 * 8)
        return 0;
    response.id = d[0] | d[1] << 8;
    response.button = d[2];
    response.list = (int16_t)(d[3] | d[4] << 8);
    len = d[5];
    if (len * 8 > bits - 48)
        len = (bits - 48) / 8;
    if (len > (int)sizeof response.text - 1)
        len = sizeof response.text - 1;
    memcpy(response.text, d + 6, len);
    response.text[len] = 0;
    response.pending = 1;
    return 1;
}

/* 0B4C samp is_dialog_active %1d% (-1: any) */
static int8_t __stdcall op_0B4C(Script *s)
{
    int id = script_int(s);
    void *dlg = dialog();
    script_cond(s, dlg && AT(dlg, DLG_ACTIVE, int) && (id < 0 || AT(dlg, DLG_ID, int) == id));
    return 0;
}

static int8_t store_field(Script *s, int off)
{
    void *dlg = dialog();
    ScriptParams[0].i = dlg ? AT(dlg, off, int) : -1;
    script_store(s, 1);
    return 0;
}

/* 0B4E samp %1d% = get_current_dialog_id, 0B4D ... type */
static int8_t __stdcall op_0B4E(Script *s) { return store_field(s, DLG_ID); }
static int8_t __stdcall op_0B4D(Script *s) { return store_field(s, DLG_TYPE); }

/* 0B3B samp show_dialog id %1d% caption %2s% text %3s% button_1 %4s% button_2 %5s% style %6d% */
static int8_t __stdcall op_0B3B(Script *s)
{
    static char caption[256], text[4096], b1[64], b2[64];
    const Build *b = samp_build();
    int id, style;
    void *dlg;
    id = script_int(s);
    copy_string(caption, script_string(s, caption, sizeof caption), sizeof caption);
    copy_string(text, script_string(s, text, sizeof text), sizeof text);
    copy_string(b1, script_string(s, b1, sizeof b1), sizeof b1);
    copy_string(b2, script_string(s, b2, sizeof b2), sizeof b2);
    style = script_int(s);
    if (!b || !(dlg = dialog()))
        return 0;
    ((ShowFn)fn(b->dialog_show))(dlg, id, style, caption, text, b1, b2, 1);
    AT(dlg, DLG_SERVERSIDE, int) = CLIENT_SIDE;
    response.pending = 0;
    return 0;
}

/* 0BD9 samp set_dialog_clientside %1d% */
static int8_t __stdcall op_0BD9(Script *s)
{
    int client = script_int(s);
    void *dlg = dialog();
    if (dlg)
        AT(dlg, DLG_SERVERSIDE, int) = client ? CLIENT_SIDE : 1;
    return 0;
}

/* 0B3C samp is_dialog_responded id %1d% button %2d% list_item %3d% input_text %4s% */
static int8_t __stdcall op_0B3C(Script *s)
{
    int id = script_int(s), hit = response.pending && response.id == id;
    char *buf;
    ScriptParams[0].i = hit ? response.button : -1;
    ScriptParams[1].i = hit ? response.list : -1;
    script_store(s, 2);
    buf = (char *)(uintptr_t)script_int(s);
    if (buf)
        copy_string(buf, hit ? response.text : "", sizeof response.text);
    if (hit)
        response.pending = 0;
    script_cond(s, hit);
    return 0;
}

/* 0BC9 samp send_dialog_response dialog %1d% button %2d% listitem %3d% input %4d% */
static int8_t __stdcall op_0BC9(Script *s)
{
    char buf[256];
    uint8_t d[6 + 255];
    const char *text;
    int id, button, list, len;
    void *rc;
    script_collect(s, 3);
    id = ScriptParams[0].i;
    button = ScriptParams[1].i;
    list = ScriptParams[2].i;
    text = script_string(s, buf, sizeof buf);
    len = lstrlenA(text);
    if (len > 255)
        len = 255;
    d[0] = (uint8_t)id; d[1] = (uint8_t)(id >> 8);
    d[2] = (uint8_t)button;
    d[3] = (uint8_t)list; d[4] = (uint8_t)(list >> 8);
    d[5] = (uint8_t)len;
    memcpy(d + 6, text, len);
    if ((rc = samp_rakclient()) != NULL) {
        typedef int (THISCALL *RpcFn)(void *, int *, void *, int, int, int, int);
        struct { int used, allocated, read; uint8_t *data; uint8_t copy, stack[1]; } __attribute__((packed)) bs;
        int rpc = RPC_DIALOG_RESPONSE;
        bs.used = bs.allocated = (6 + len) * 8;
        bs.read = 0;
        bs.data = d;
        bs.copy = 0;
        sending = 1;
        ((RpcFn)(*(void ***)rc)[25])(rc, &rpc, &bs, 1, 9, 0, 0);
        sending = 0;
    }
    return 0;
}

/* 0B47 samp close_current_dialog_with_button %1d% */
static int8_t __stdcall op_0B47(Script *s)
{
    int button = script_int(s);
    const Build *b = samp_build();
    void *dlg = dialog();
    if (b && dlg && AT(dlg, DLG_ACTIVE, int))
        ((CloseFn)fn(b->dialog_close))(dlg, (char)button);
    return 0;
}

/* 0B48 samp %1d% = get_current_dialog_list_item */
static int8_t __stdcall op_0B48(Script *s)
{
    const Build *b = samp_build();
    void *dlg = dialog(), *lb;
    ScriptParams[0].i = b && dlg && (lb = AT(dlg, DLG_LISTBOX, void *))
        ? ((SelectedFn)fn(b->listbox_selected))(lb, -1) : -1;
    script_store(s, 1);
    return 0;
}

/* 0B49 samp set_current_dialog_list_item %1d% */
static int8_t __stdcall op_0B49(Script *s)
{
    int item = script_int(s);
    const Build *b = samp_build();
    void *dlg = dialog(), *lb;
    if (b && dlg && (lb = AT(dlg, DLG_LISTBOX, void *)))
        ((SelectFn)fn(b->listbox_select))(lb, item);
    return 0;
}

/* 0B4A samp %1d% = get_current_dialog_editbox_text: into MoonLoader's buffer */
static int8_t __stdcall op_0B4A(Script *s)
{
    char *buf = (char *)(uintptr_t)script_int(s);
    const Build *b = samp_build();
    void *dlg = dialog(), *eb;
    if (buf)
        copy_string(buf, b && dlg && (eb = AT(dlg, DLG_EDITBOX, void *))
                    ? ((GetTextFn)fn(b->editbox_get))(eb) : "", 256);
    return 0;
}

/* 0B4B samp set_current_dialog_editbox_text %1d% */
static int8_t __stdcall op_0B4B(Script *s)
{
    char buf[256];
    const char *text = script_string(s, buf, sizeof buf);
    const Build *b = samp_build();
    void *dlg = dialog(), *eb;
    copy_string(buf, text, sizeof buf);
    if (b && dlg && (eb = AT(dlg, DLG_EDITBOX, void *)))
        ((SetTextFn)fn(b->editbox_set))(eb, buf, 0);
    return 0;
}

/* 0BDA samp is_dialog_clientside: not answered to the server */
static int8_t __stdcall op_0BDA(Script *s)
{
    void *dlg = dialog();
    script_cond(s, !dlg || AT(dlg, DLG_SERVERSIDE, int) != 1);
    return 0;
}

void dialogs_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B4C, op_0B4C }, { 0x0B4E, op_0B4E }, { 0x0B4D, op_0B4D }, { 0x0B3B, op_0B3B },
        { 0x0BD9, op_0BD9 }, { 0x0B3C, op_0B3C }, { 0x0BC9, op_0BC9 }, { 0x0B47, op_0B47 },
        { 0x0B48, op_0B48 }, { 0x0B49, op_0B49 }, { 0x0B4A, op_0B4A }, { 0x0B4B, op_0B4B },
        { 0x0BDA, op_0BDA },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
