/* SAMPFUNCS's script dialogs (dxut*, 0B80-0BAB), as DXUT's: a dialog's
 * position is its client area's top left, its caption sits above it, and
 * clicking the caption minimises it (dragging moves it). Controls sit at
 * client coordinates and report DXUT's events, popped by 0B81. A dialog
 * takes the mouse only while SA-MP's cursor is shown, and the keyboard while
 * one of its edit boxes has focus. */
#include "moonsamp.h"

#define MAX_DIALOGS   32
#define MAX_CONTROLS  64
#define MAX_EVENTS    64

#define EVENT_BUTTON_CLICKED        0x0101
#define EVENT_CHECKBOX_CHANGED      0x0401
#define EVENT_SLIDER_VALUE_CHANGED  0x0501
#define EVENT_EDITBOX_STRING        0x0601
#define EVENT_EDITBOX_CHANGE        0x0602
#define EVENT_LISTBOX_ITEM_DBLCLK   0x0701
#define EVENT_LISTBOX_SELECTION     0x0702

#define COLOR_TEXT     0xFFFFFFFFu
#define COLOR_BORDER   0xFF6E6E6Eu
#define COLOR_FIELD    0xC8101010u
#define COLOR_FACE     0xC8303030u
#define COLOR_HOVER    0xC8484848u
#define COLOR_DOWN     0xC8646464u
#define COLOR_SELECTED 0xC8405A96u
#define COLOR_CAPTION  0xF0222222u
#define COLOR_DIALOG   0xB4000000u
#define SCROLLBAR_W    12

enum { CT_BUTTON, CT_CHECKBOX, CT_STATIC, CT_EDITBOX, CT_SLIDER, CT_LISTBOX };

typedef struct { char *text; uint32_t data; } Item;

typedef struct {
    int used, type, id, x, y, w, h, visible;
    EditLine edit;                      /* the text of every kind */
    uint32_t color;                     /* a checkbox's text */
    int checked, value, max;
    Item *items;
    int count, cap, selected, top;
} Control;

typedef struct {
    int used, x, y, w, h, visible, caption, minimized;
    char name[128];
    uint32_t bg;
    Control controls[MAX_CONTROLS];
    struct { int event, id; } events[MAX_EVENTS];
    int ev_first, ev_count;
    int focus, pressed;                 /* control indexes, or -1 */
    int drag, drag_moved, drag_x, drag_y;
    DWORD last_click;
    int last_click_item;
} Dialog;

static Dialog *dialogs[MAX_DIALOGS];   /* drawn in order: the last on top */

static void *alloc(size_t n) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n); }
static void release(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }

static int caption_h(void) { return overlay_line_height(FONT_UI) + 6; }
static int row_h(void) { return overlay_line_height(FONT_UI) + 2; }

static Dialog *dialog_arg(Script *s)
{
    Dialog *d = (Dialog *)(uintptr_t)script_int(s);
    int i;
    for (i = 0; i < MAX_DIALOGS; i++)
        if (d && dialogs[i] == d)
            return d;
    return NULL;
}

static Control *find(Dialog *d, int id)
{
    int i;
    for (i = 0; d && i < MAX_CONTROLS; i++)
        if (d->controls[i].used && d->controls[i].id == id)
            return &d->controls[i];
    return NULL;
}

static Control *control_arg(Script *s, Dialog **dp)
{
    Dialog *d = dialog_arg(s);
    int id = script_int(s);
    if (dp)
        *dp = d;
    return find(d, id);
}

static void push(Dialog *d, int event, int id)
{
    if (d->ev_count == MAX_EVENTS) {
        d->ev_first = (d->ev_first + 1) % MAX_EVENTS;
        d->ev_count--;
    }
    d->events[(d->ev_first + d->ev_count) % MAX_EVENTS].event = event;
    d->events[(d->ev_first + d->ev_count) % MAX_EVENTS].id = id;
    d->ev_count++;
}

static void free_items(Control *c)
{
    int i;
    for (i = 0; i < c->count; i++)
        release(c->items[i].text);
    release(c->items);
    c->items = NULL;
    c->count = c->cap = 0;
}

static void delete_dialog(int i)
{
    int k;
    for (k = 0; k < MAX_CONTROLS; k++)
        free_items(&dialogs[i]->controls[k]);
    release(dialogs[i]);
    for (k = i; k < MAX_DIALOGS - 1; k++)
        dialogs[k] = dialogs[k + 1];
    dialogs[MAX_DIALOGS - 1] = NULL;
}

static void bring_front(Dialog *d)
{
    int i, j;
    for (i = 0; i < MAX_DIALOGS && dialogs[i] != d; i++)
        ;
    for (j = i; j < MAX_DIALOGS - 1 && dialogs[j + 1]; j++)
        dialogs[j] = dialogs[j + 1];
    if (i < MAX_DIALOGS)
        dialogs[j] = d;
}

/* ---- drawing ------------------------------------------------------------ */

static int visible_rows(Control *c)
{
    int n = (c->h - 4) / row_h();
    return n > 0 ? n : 1;
}

static void clamp_top(Control *c)
{
    int most = c->count - visible_rows(c);
    if (c->top > most)
        c->top = most;
    if (c->top < 0)
        c->top = 0;
}

static int inside(int px, int py, int x, int y, int w, int h)
{
    return px >= x && py >= y && px < x + w && py < y + h;
}

static void draw_centered(const char *text, int x, int y, int w, int h, uint32_t color)
{
    int tw = overlay_text_width(FONT_UI, text, -1);
    overlay_text(FONT_UI, x + (w > tw ? (w - tw) / 2 : 0), y + (h - overlay_line_height(FONT_UI)) / 2, text, color, w);
}

static void draw_control(Dialog *d, int k, int mx, int my)
{
    Control *c = &d->controls[k];
    int x = d->x + c->x, y = d->y + c->y, lh = overlay_line_height(FONT_UI), i;
    int hover = inside(mx, my, x, y, c->w, c->h);
    switch (c->type) {
    case CT_BUTTON:
        overlay_rect(x, y, c->w, c->h, d->pressed == k && hover ? COLOR_DOWN : hover ? COLOR_HOVER : COLOR_FACE);
        overlay_frame(x, y, c->w, c->h, COLOR_BORDER);
        draw_centered(c->edit.text, x, y, c->w, c->h, COLOR_TEXT);
        break;
    case CT_CHECKBOX: {
        int box = c->h < lh + 4 ? c->h : lh + 4, by = y + (c->h - box) / 2;
        overlay_rect(x, by, box, box, hover ? COLOR_HOVER : COLOR_FIELD);
        overlay_frame(x, by, box, box, COLOR_BORDER);
        if (c->checked)
            overlay_rect(x + 3, by + 3, box - 6, box - 6, c->color);
        overlay_text(FONT_UI, x + box + 4, y + (c->h - lh) / 2, c->edit.text, c->color, c->w - box - 4);
        break;
    }
    case CT_STATIC:
        draw_centered(c->edit.text, x, y, c->w, c->h, COLOR_TEXT);
        break;
    case CT_EDITBOX:
        overlay_rect(x, y, c->w, c->h, COLOR_FIELD);
        overlay_frame(x, y, c->w, c->h, d->focus == k ? COLOR_TEXT : COLOR_BORDER);
        edit_draw(&c->edit, FONT_UI, x + 4, y + (c->h - lh) / 2, c->w - 8, COLOR_TEXT, d->focus == k);
        break;
    case CT_SLIDER: {
        int thumb = c->h / 2 > 6 ? c->h / 2 : 6;
        int tx = x + (c->max > 0 ? (c->w - thumb) * c->value / c->max : 0);
        overlay_rect(x, y + c->h / 2 - 2, c->w, 4, COLOR_FIELD);
        overlay_frame(x, y + c->h / 2 - 2, c->w, 4, COLOR_BORDER);
        overlay_rect(tx, y, thumb, c->h, d->pressed == k ? COLOR_DOWN : hover ? COLOR_HOVER : COLOR_FACE);
        overlay_frame(tx, y, thumb, c->h, COLOR_BORDER);
        break;
    }
    case CT_LISTBOX: {
        int rows = visible_rows(c), tw = c->w - 4 - (c->count > rows ? SCROLLBAR_W : 0);
        overlay_rect(x, y, c->w, c->h, COLOR_FIELD);
        overlay_frame(x, y, c->w, c->h, d->focus == k ? COLOR_TEXT : COLOR_BORDER);
        clamp_top(c);
        for (i = 0; i < rows && c->top + i < c->count; i++) {
            int ry = y + 2 + i * row_h();
            if (c->top + i == c->selected)
                overlay_rect(x + 2, ry, tw, row_h(), COLOR_SELECTED);
            overlay_text(FONT_UI, x + 4, ry + 1, c->items[c->top + i].text, COLOR_TEXT, tw - 4);
        }
        if (c->count > rows) {
            int sx = x + c->w - SCROLLBAR_W - 1, track = c->h - 2, th = track * rows / c->count;
            if (th < 8)
                th = 8;
            overlay_rect(sx, y + 1, SCROLLBAR_W, track, COLOR_FACE);
            overlay_rect(sx + 2, y + 1 + (track - th) * c->top / (c->count - rows), SCROLLBAR_W - 4, th, COLOR_DOWN);
        }
        break;
    }
    }
}

int dxut_visible(void)
{
    int i;
    for (i = 0; i < MAX_DIALOGS && dialogs[i]; i++)
        if (dialogs[i]->visible)
            return 1;
    return 0;
}

void dxut_render(void)
{
    int i, k, mx, my;
    overlay_cursor(&mx, &my);
    if (samp_cursor_mode() == 0)
        mx = my = -1;
    for (i = 0; i < MAX_DIALOGS && dialogs[i]; i++) {
        Dialog *d = dialogs[i];
        if (!d->visible)
            continue;
        if (d->caption) {
            int ch = caption_h();
            overlay_rect(d->x, d->y - ch, d->w, ch, COLOR_CAPTION);
            overlay_text(FONT_UI, d->x + 5, d->y - ch + 3, d->name, COLOR_TEXT, d->w - 10);
        }
        if (d->minimized)
            continue;
        overlay_rect(d->x, d->y, d->w, d->h, d->bg);
        for (k = 0; k < MAX_CONTROLS; k++)
            if (d->controls[k].used && d->controls[k].visible)
                draw_control(d, k, mx, my);
    }
}

/* ---- input ---------------------------------------------------------------- */

static int hit_control(Dialog *d, int mx, int my)
{
    int k;
    for (k = MAX_CONTROLS - 1; k >= 0; k--) {
        Control *c = &d->controls[k];
        if (c->used && c->visible && inside(mx, my, d->x + c->x, d->y + c->y, c->w, c->h))
            return k;
    }
    return -1;
}

static void set_slider(Dialog *d, Control *c, int mx)
{
    int thumb = c->h / 2 > 6 ? c->h / 2 : 6, span = c->w - thumb, v;
    v = span > 0 ? ((mx - d->x - c->x - thumb / 2) * c->max + span / 2) / span : 0;
    v = v < 0 ? 0 : v > c->max ? c->max : v;
    if (v != c->value) {
        c->value = v;
        push(d, EVENT_SLIDER_VALUE_CHANGED, c->id);
    }
}

static void select_item(Dialog *d, Control *c, int item)
{
    if (item < 0 || item >= c->count || item == c->selected)
        return;
    c->selected = item;
    if (item < c->top)
        c->top = item;
    else if (item >= c->top + visible_rows(c))
        c->top = item - visible_rows(c) + 1;
    push(d, EVENT_LISTBOX_SELECTION, c->id);
}

static void listbox_click(Dialog *d, Control *c, int mx, int my)
{
    int rows = visible_rows(c), y = d->y + c->y, item;
    DWORD now = GetTickCount();
    if (c->count > rows && mx >= d->x + c->x + c->w - SCROLLBAR_W - 1) {
        c->top += my < y + c->h / 2 ? -rows : rows;
        clamp_top(c);
        return;
    }
    item = c->top + (my - y - 2) / row_h();
    if (item >= c->count)
        return;
    if (item == d->last_click_item && now - d->last_click < GetDoubleClickTime()) {
        push(d, EVENT_LISTBOX_ITEM_DBLCLK, c->id);
        d->last_click = 0;
        return;
    }
    d->last_click = now;
    d->last_click_item = item;
    select_item(d, c, item);
}

static Dialog *dialog_at(int mx, int my)
{
    int i;
    for (i = MAX_DIALOGS - 1; i >= 0; i--) {
        Dialog *d = dialogs[i];
        int top = d && d->caption ? d->y - caption_h() : d ? d->y : 0;
        if (d && d->visible && inside(mx, my, d->x, top, d->w, (d->minimized ? 0 : d->h) + (d->y - top)))
            return d;
    }
    return NULL;
}

static Dialog *focused(void)
{
    int i;
    for (i = MAX_DIALOGS - 1; i >= 0; i--)
        if (dialogs[i] && dialogs[i]->visible && !dialogs[i]->minimized && dialogs[i]->focus >= 0)
            return dialogs[i];
    return NULL;
}

static void clear_focus(void)
{
    int i;
    for (i = 0; i < MAX_DIALOGS && dialogs[i]; i++)
        dialogs[i]->focus = -1;
}

static int keyboard(UINT msg, WPARAM wp)
{
    Dialog *d = focused();
    Control *c = d ? &d->controls[d->focus] : NULL;
    int r;
    if (!c || !c->used || !c->visible || (msg != WM_CHAR && msg != WM_KEYDOWN && msg != WM_KEYUP))
        return 0;
    if (c->type == CT_LISTBOX) {
        if (msg == WM_KEYDOWN && (wp == VK_UP || wp == VK_DOWN))
            select_item(d, c, c->selected < 0 ? 0 : c->selected + (wp == VK_UP ? -1 : 1));
        else if (msg == WM_KEYDOWN && wp == VK_RETURN)
            push(d, EVENT_LISTBOX_ITEM_DBLCLK, c->id);
        else if (msg == WM_CHAR && wp == 0x1B)
            d->focus = -1;
        else
            return 0;
        return 1;
    }
    if (c->type != CT_EDITBOX)
        return 0;
    if (msg == WM_CHAR && wp == 0x1B) {
        d->focus = -1;
        return 1;
    }
    r = edit_key(&c->edit, msg, wp);
    if (r & EDIT_CHANGED)
        push(d, EVENT_EDITBOX_CHANGE, c->id);
    if (r & EDIT_ENTER)
        push(d, EVENT_EDITBOX_STRING, c->id);
    return r & EDIT_HANDLED;
}

static Dialog *pressed_in(void)
{
    int i;
    for (i = 0; i < MAX_DIALOGS && dialogs[i]; i++)
        if (dialogs[i]->pressed >= 0 || dialogs[i]->drag)
            return dialogs[i];
    return NULL;
}

int dxut_input(UINT msg, WPARAM wp, LPARAM lp)
{
    Dialog *d;
    Control *c;
    int mx, my, k;
    if (msg >= WM_KEYFIRST && msg <= WM_KEYLAST)
        return keyboard(msg, wp);
    if (msg < WM_MOUSEFIRST || msg > WM_MOUSELAST)
        return 0;
    overlay_point(msg, lp, &mx, &my);
    if ((d = pressed_in())) {
        c = d->pressed >= 0 ? &d->controls[d->pressed] : NULL;
        if (msg == WM_MOUSEMOVE) {
            if (d->drag) {
                d->drag_moved |= mx != d->drag_x || my != d->drag_y;
                d->x += mx - d->drag_x;
                d->y += my - d->drag_y;
                d->drag_x = mx;
                d->drag_y = my;
            } else if (c && c->type == CT_SLIDER) {
                set_slider(d, c, mx);
            }
            return 1;
        }
        if (msg == WM_LBUTTONUP) {
            if (d->drag && !d->drag_moved)
                d->minimized = !d->minimized;
            else if (c && c->used && inside(mx, my, d->x + c->x, d->y + c->y, c->w, c->h)) {
                if (c->type == CT_BUTTON)
                    push(d, EVENT_BUTTON_CLICKED, c->id);
                else if (c->type == CT_CHECKBOX) {
                    c->checked = !c->checked;
                    push(d, EVENT_CHECKBOX_CHANGED, c->id);
                }
            }
            d->drag = 0;
            d->pressed = -1;
            ReleaseCapture();
            return 1;
        }
    }
    if (samp_cursor_mode() == 0 || !(d = dialog_at(mx, my))) {
        if (msg == WM_LBUTTONDOWN)
            clear_focus();
        return 0;
    }
    if (msg == WM_MOUSEWHEEL) {
        k = hit_control(d, mx, my);
        if (k >= 0 && d->controls[k].type == CT_LISTBOX) {
            d->controls[k].top -= (short)HIWORD(wp) / WHEEL_DELTA * 3;
            clamp_top(&d->controls[k]);
        }
        return 1;
    }
    if (msg != WM_LBUTTONDOWN && msg != WM_LBUTTONDBLCLK)
        return msg != WM_MOUSEMOVE;
    bring_front(d);
    clear_focus();
    SetCapture(overlay_window());
    if (d->caption && my < d->y) {
        d->drag = 1;
        d->drag_moved = 0;
        d->drag_x = mx;
        d->drag_y = my;
        return 1;
    }
    if ((k = hit_control(d, mx, my)) < 0)
        return 1;
    c = &d->controls[k];
    d->pressed = k;
    if (c->type == CT_EDITBOX || c->type == CT_LISTBOX)
        d->focus = k;
    if (c->type == CT_SLIDER)
        set_slider(d, c, mx);
    else if (c->type == CT_LISTBOX)
        listbox_click(d, c, mx, my);
    return 1;
}

/* ---- opcodes ------------------------------------------------------------ */

static void store(int32_t a)
{
    ScriptParams[0].i = a;
}

/* 0B80 dialog %2d% = create %1s% */
static int8_t __stdcall op_0B80(Script *s)
{
    char name[128];
    Dialog *d = NULL;
    int i, k;
    copy_string(name, script_string(s, name, sizeof name), sizeof name);
    for (i = 0; i < MAX_DIALOGS && dialogs[i]; i++)
        ;
    if (i < MAX_DIALOGS && (d = alloc(sizeof *d))) {
        copy_string(d->name, name, sizeof d->name);
        d->used = d->visible = d->caption = 1;
        d->bg = COLOR_DIALOG;
        d->focus = d->pressed = -1;
        for (k = 0; k < MAX_CONTROLS; k++)
            d->controls[k].selected = -1;
        dialogs[i] = d;
    }
    store((int32_t)(uintptr_t)d);
    script_store(s, 1);
    return 0;
}

/* 0B81 dialog %1d% pop_event_to %2d% control_id_to %3d% */
static int8_t __stdcall op_0B81(Script *s)
{
    Dialog *d = dialog_arg(s);
    int ok = d && d->ev_count;
    ScriptParams[0].i = ok ? d->events[d->ev_first].event : 0;
    ScriptParams[1].i = ok ? d->events[d->ev_first].id : 0;
    if (ok) {
        d->ev_first = (d->ev_first + 1) % MAX_EVENTS;
        d->ev_count--;
    }
    script_store(s, 2);
    script_cond(s, ok);
    return 0;
}

static Control *add(Script *s, int type, int has_text)
{
    char text[256] = "";
    Dialog *d = dialog_arg(s);
    Control *c = NULL;
    int id = script_int(s), k;
    if (has_text)
        copy_string(text, script_string(s, text, sizeof text), sizeof text);
    script_collect(s, 4);
    for (k = 0; d && k < MAX_CONTROLS && d->controls[k].used; k++)
        ;
    if (!d || k == MAX_CONTROLS || find(d, id))
        return NULL;
    c = &d->controls[k];
    memset(c, 0, sizeof *c);
    c->used = c->visible = 1;
    c->type = type;
    c->id = id;
    c->x = ScriptParams[0].i;
    c->y = ScriptParams[1].i;
    c->w = ScriptParams[2].i;
    c->h = ScriptParams[3].i;
    c->color = COLOR_TEXT;
    c->selected = -1;
    edit_set(&c->edit, text);
    return c;
}

/* 0B82 add_button, 0B83 add_checkbox, 0B88 add_editbox, 0B91 add_static:
 * dialog id text x y w h */
static int8_t __stdcall op_0B82(Script *s) { add(s, CT_BUTTON, 1); return 0; }
static int8_t __stdcall op_0B83(Script *s) { add(s, CT_CHECKBOX, 1); return 0; }
static int8_t __stdcall op_0B88(Script *s) { add(s, CT_EDITBOX, 1); return 0; }
static int8_t __stdcall op_0B91(Script *s) { add(s, CT_STATIC, 1); return 0; }

/* 0B96 add_slider id x y w h max */
static int8_t __stdcall op_0B96(Script *s)
{
    Control *c = add(s, CT_SLIDER, 0);
    int max = script_int(s);
    if (c)
        c->max = max > 0 ? max : 0;
    return 0;
}

/* 0B99 add_listbox id x y w h */
static int8_t __stdcall op_0B99(Script *s) { add(s, CT_LISTBOX, 0); return 0; }

/* 0B84 dialog %1d% set_pos_XY %2d% %3d% size %4d% %5d% */
static int8_t __stdcall op_0B84(Script *s)
{
    Dialog *d = dialog_arg(s);
    script_collect(s, 4);
    if (d) {
        d->x = ScriptParams[0].i;
        d->y = ScriptParams[1].i;
        d->w = ScriptParams[2].i;
        d->h = ScriptParams[3].i;
    }
    return 0;
}

/* 0B85 dialog %1d% get_position_to %2d% %3d% size_to %4d% %5d% */
static int8_t __stdcall op_0B85(Script *s)
{
    Dialog *d = dialog_arg(s);
    ScriptParams[0].i = d ? d->x : 0;
    ScriptParams[1].i = d ? d->y : 0;
    ScriptParams[2].i = d ? d->w : 0;
    ScriptParams[3].i = d ? d->h : 0;
    script_store(s, 4);
    return 0;
}

/* Dialog flags: 0B86 / 0B87 visible, 0B9F / 0BA0 caption, 0BA1 / 0BA2 minimized */
static int8_t set_flag(Script *s, size_t off)
{
    Dialog *d = dialog_arg(s);
    int v = script_int(s) != 0;
    if (d)
        AT(d, off, int) = v;
    if (d && off == __builtin_offsetof(Dialog, visible) && !v)
        d->focus = -1;
    return 0;
}

static int8_t get_flag(Script *s, size_t off)
{
    Dialog *d = dialog_arg(s);
    script_cond(s, d && AT(d, off, int));
    return 0;
}

static int8_t __stdcall op_0B86(Script *s) { return set_flag(s, __builtin_offsetof(Dialog, visible)); }
static int8_t __stdcall op_0B87(Script *s) { return get_flag(s, __builtin_offsetof(Dialog, visible)); }
static int8_t __stdcall op_0B9F(Script *s) { return set_flag(s, __builtin_offsetof(Dialog, caption)); }
static int8_t __stdcall op_0BA0(Script *s) { return get_flag(s, __builtin_offsetof(Dialog, caption)); }
static int8_t __stdcall op_0BA1(Script *s) { return set_flag(s, __builtin_offsetof(Dialog, minimized)); }
static int8_t __stdcall op_0BA2(Script *s) { return get_flag(s, __builtin_offsetof(Dialog, minimized)); }

/* 0B93 dialog %1d% set_background_color %2d% */
static int8_t __stdcall op_0B93(Script *s)
{
    Dialog *d = dialog_arg(s);
    uint32_t color = (uint32_t)script_int(s);
    if (d)
        d->bg = color;
    return 0;
}

/* 0B89 dialog %1d% get_control %2d% text_to %3d%: into MoonLoader's buffer */
static int8_t __stdcall op_0B89(Script *s)
{
    Control *c = control_arg(s, NULL);
    char *buf = (char *)(uintptr_t)script_int(s);
    if (buf)
        copy_string(buf, c ? c->edit.text : "", ML_BUFFER);
    return 0;
}

/* 0B94 dialog %1d% set_control %2d% text %3d% */
static int8_t __stdcall op_0B94(Script *s)
{
    char text[256];
    Control *c = control_arg(s, NULL);
    copy_string(text, script_string(s, text, sizeof text), sizeof text);
    if (c)
        edit_set(&c->edit, text);
    return 0;
}

/* 0B90 dialog %1d% control %2d% set_visible %3d% */
static int8_t __stdcall op_0B90(Script *s)
{
    Dialog *d;
    Control *c = control_arg(s, &d);
    int v = script_int(s) != 0;
    if (c) {
        c->visible = v;
        if (!v && d->focus == c - d->controls)
            d->focus = -1;
    }
    return 0;
}

/* 0B95 dialog %1d% control %2d% is_visible */
static int8_t __stdcall op_0B95(Script *s)
{
    Control *c = control_arg(s, NULL);
    script_cond(s, c && c->visible);
    return 0;
}

/* 0B92 dialog %1d% checkbox %2d% is_checked */
static int8_t __stdcall op_0B92(Script *s)
{
    Control *c = control_arg(s, NULL);
    script_cond(s, c && c->type == CT_CHECKBOX && c->checked);
    return 0;
}

/* 0B9E dialog %1d% checkbox %2d% set_checked %3d%: no event, as DXUT */
static int8_t __stdcall op_0B9E(Script *s)
{
    Control *c = control_arg(s, NULL);
    int v = script_int(s) != 0;
    if (c && c->type == CT_CHECKBOX)
        c->checked = v;
    return 0;
}

/* 0BAA dialog %1d% set_checkbox %2d% color %3d%: its text and tick */
static int8_t __stdcall op_0BAA(Script *s)
{
    Control *c = control_arg(s, NULL);
    uint32_t color = (uint32_t)script_int(s);
    if (c && c->type == CT_CHECKBOX)
        c->color = color;
    return 0;
}

/* 0B97 dialog %1d% slider %2d% get_value_to %3d% */
static int8_t __stdcall op_0B97(Script *s)
{
    Control *c = control_arg(s, NULL);
    store(c && c->type == CT_SLIDER ? c->value : 0);
    script_store(s, 1);
    return 0;
}

/* 0B98 dialog %1d% slider %2d% set_value_to %3d%: no event, as DXUT */
static int8_t __stdcall op_0B98(Script *s)
{
    Control *c = control_arg(s, NULL);
    int v = script_int(s);
    if (c && c->type == CT_SLIDER)
        c->value = v < 0 ? 0 : v > c->max ? c->max : v;
    return 0;
}

/* 0B9A dialog %1d% listbox %2d% insert_element %3d% with_data %4d% after %5d%:
 * at that index, or last */
static int8_t __stdcall op_0B9A(Script *s)
{
    char text[256];
    Control *c = control_arg(s, NULL);
    uint32_t data;
    int at, i;
    copy_string(text, script_string(s, text, sizeof text), sizeof text);
    data = (uint32_t)script_int(s);
    at = script_int(s);
    if (!c || c->type != CT_LISTBOX)
        return 0;
    if (c->count == c->cap) {
        int cap = c->cap ? c->cap * 2 : 16;
        Item *items = alloc(cap * sizeof *items);
        if (!items)
            return 0;
        if (c->items)
            memcpy(items, c->items, c->count * sizeof *items);
        release(c->items);
        c->items = items;
        c->cap = cap;
    }
    if (at < 0 || at > c->count)
        at = c->count;
    for (i = c->count; i > at; i--)
        c->items[i] = c->items[i - 1];
    if (!(c->items[at].text = alloc(lstrlenA(text) + 1)))
        return 0;
    lstrcpyA(c->items[at].text, text);
    c->items[at].data = data;
    c->count++;
    if (c->selected >= at)
        c->selected++;
    return 0;
}

/* 0B9B dialog %1d% listbox %2d% selected_element_to %3d% elements_count_to %4d% */
static int8_t __stdcall op_0B9B(Script *s)
{
    Control *c = control_arg(s, NULL);
    int ok = c && c->type == CT_LISTBOX;
    ScriptParams[0].i = ok ? c->selected : -1;
    ScriptParams[1].i = ok ? c->count : 0;
    script_store(s, 2);
    return 0;
}

/* 0B9C dialog %1d% listbox %2d% delete_element %3d% */
static int8_t __stdcall op_0B9C(Script *s)
{
    Control *c = control_arg(s, NULL);
    int at = script_int(s), i;
    if (!c || c->type != CT_LISTBOX || at < 0 || at >= c->count)
        return 0;
    release(c->items[at].text);
    for (i = at; i < c->count - 1; i++)
        c->items[i] = c->items[i + 1];
    c->count--;
    if (c->selected == at)
        c->selected = -1;
    else if (c->selected > at)
        c->selected--;
    clamp_top(c);
    return 0;
}

/* 0B9D dialog %1d% listbox %2d% element %3d% store_text_to %4d% data_to %5d%:
 * the text into MoonLoader's buffer */
static int8_t __stdcall op_0B9D(Script *s)
{
    Control *c = control_arg(s, NULL);
    int at = script_int(s), ok = c && c->type == CT_LISTBOX && at >= 0 && at < c->count;
    char *buf = (char *)(uintptr_t)script_int(s);
    if (buf)
        copy_string(buf, ok ? c->items[at].text : "", ML_BUFFER);
    ScriptParams[0].u = ok ? c->items[at].data : 0;
    script_store(s, 1);
    return 0;
}

/* 0BA3 dialog %1d% control %2d% free */
static int8_t __stdcall op_0BA3(Script *s)
{
    Dialog *d;
    Control *c = control_arg(s, &d);
    if (c) {
        int k = c - d->controls;
        free_items(c);
        c->used = 0;
        if (d->focus == k)
            d->focus = -1;
        if (d->pressed == k)
            d->pressed = -1;
    }
    return 0;
}

/* 0BA4 dialog %1d% free */
static int8_t __stdcall op_0BA4(Script *s)
{
    Dialog *d = dialog_arg(s);
    int i;
    for (i = 0; d && i < MAX_DIALOGS; i++)
        if (dialogs[i] == d) {
            if (d->pressed >= 0 || d->drag)
                ReleaseCapture();
            delete_dialog(i);
            break;
        }
    return 0;
}

/* 0BA5 dialog %1d% set_focus_on_control %2d% */
static int8_t __stdcall op_0BA5(Script *s)
{
    Dialog *d;
    Control *c = control_arg(s, &d);
    if (c) {
        clear_focus();
        d->focus = c - d->controls;
        bring_front(d);
    }
    return 0;
}

/* 0BA6 set_control size, 0BA8 set_control pos */
static int8_t set_pair(Script *s, int pos)
{
    Control *c = control_arg(s, NULL);
    script_collect(s, 2);
    if (c && pos) {
        c->x = ScriptParams[0].i;
        c->y = ScriptParams[1].i;
    } else if (c) {
        c->w = ScriptParams[0].i;
        c->h = ScriptParams[1].i;
    }
    return 0;
}

static int8_t get_pair(Script *s, int pos)
{
    Control *c = control_arg(s, NULL);
    ScriptParams[0].i = !c ? 0 : pos ? c->x : c->w;
    ScriptParams[1].i = !c ? 0 : pos ? c->y : c->h;
    script_store(s, 2);
    return 0;
}

static int8_t __stdcall op_0BA6(Script *s) { return set_pair(s, 0); }
static int8_t __stdcall op_0BA7(Script *s) { return get_pair(s, 0); }
static int8_t __stdcall op_0BA8(Script *s) { return set_pair(s, 1); }
static int8_t __stdcall op_0BA9(Script *s) { return get_pair(s, 1); }

/* 0BAB dialog %1d% is_exists */
static int8_t __stdcall op_0BAB(Script *s)
{
    script_cond(s, dialog_arg(s) != NULL);
    return 0;
}

void dxut_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B80, op_0B80 }, { 0x0B81, op_0B81 }, { 0x0B82, op_0B82 }, { 0x0B83, op_0B83 },
        { 0x0B84, op_0B84 }, { 0x0B85, op_0B85 }, { 0x0B86, op_0B86 }, { 0x0B87, op_0B87 },
        { 0x0B88, op_0B88 }, { 0x0B89, op_0B89 }, { 0x0B90, op_0B90 }, { 0x0B91, op_0B91 },
        { 0x0B92, op_0B92 }, { 0x0B93, op_0B93 }, { 0x0B94, op_0B94 }, { 0x0B95, op_0B95 },
        { 0x0B96, op_0B96 }, { 0x0B97, op_0B97 }, { 0x0B98, op_0B98 }, { 0x0B99, op_0B99 },
        { 0x0B9A, op_0B9A }, { 0x0B9B, op_0B9B }, { 0x0B9C, op_0B9C }, { 0x0B9D, op_0B9D },
        { 0x0B9E, op_0B9E }, { 0x0B9F, op_0B9F }, { 0x0BA0, op_0BA0 }, { 0x0BA1, op_0BA1 },
        { 0x0BA2, op_0BA2 }, { 0x0BA3, op_0BA3 }, { 0x0BA4, op_0BA4 }, { 0x0BA5, op_0BA5 },
        { 0x0BA6, op_0BA6 }, { 0x0BA7, op_0BA7 }, { 0x0BA8, op_0BA8 }, { 0x0BA9, op_0BA9 },
        { 0x0BAA, op_0BAA }, { 0x0BAB, op_0BAB },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
