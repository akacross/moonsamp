/* What moonsamp draws in game: script dialogs (dxut.c) and the console
 * (console.c). They are drawn on SA-MP's real Direct3D device, in its
 * Present, so after SA-MP's own UI; and they see window input before SA-MP.
 *
 * Text takes SA-MP's colour codes: {RRGGBB} changes the colour, keeping the
 * alpha; not with TEXT_RAW. Coordinates are back buffer pixels. */
#define COBJMACROS
#include "moonsamp.h"
#include <d3d9.h>
#include <d3dx9core.h>

typedef HRESULT (WINAPI *PresentFn)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef HRESULT (WINAPI *ResetFn)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
typedef HRESULT (WINAPI *CreateFontFn)(IDirect3DDevice9 *, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD,
                                       DWORD, LPCSTR, ID3DXFont **);

typedef struct { float x, y, z, rhw; DWORD color; } Vertex;

#define SLOT_RESET   16
#define SLOT_PRESENT 17
#define RS_GLOBAL_PS 0xC17054       /* RsGlobal.ps; its window first */

static const struct { const char *face; int size, weight; } spec[FONT_COUNT] = {
    { "Arial", 16, FW_BOLD },           /* FONT_UI, at 1080 lines */
    { "Lucida Console", 15, FW_NORMAL },
};

static IDirect3DDevice9 *dev;
static PresentFn orig_present;
static ResetFn orig_reset;
static CreateFontFn create_font;
static ID3DXFont *fonts[FONT_COUNT];
static int line_h[FONT_COUNT];
static IDirect3DStateBlock9 *saved;
static int width, height, font_basis, rect_ready;
static HWND window;
static WNDPROC orig_wndproc;
static DWORD wndproc_wait;

int overlay_width(void) { return width; }
int overlay_height(void) { return height; }

static ID3DXFont *font(int f)
{
    static int loaded;
    if (!fonts[f] && !loaded) {
        HMODULE m = LoadLibraryA("d3dx9_43.dll");
        loaded = 1;
        create_font = m ? (CreateFontFn)(void *)GetProcAddress(m, "D3DXCreateFontA") : NULL;
        if (!create_font)
            ms_log("overlay: d3dx9_43.dll has no D3DXCreateFontA, no text");
    }
    if (!fonts[f] && create_font && height) {
        int size = spec[f].size * height / 1080;
        TEXTMETRICA tm;
        if (size < 12)
            size = 12;
        if (FAILED(create_font(dev, size, 0, spec[f].weight, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, spec[f].face, &fonts[f]))) {
            fonts[f] = NULL;
            create_font = NULL;
            ms_log("overlay: D3DXCreateFontA failed, no text");
            return NULL;
        }
        line_h[f] = SUCCEEDED(ID3DXFont_GetTextMetricsA(fonts[f], &tm)) ? tm.tmHeight : size;
    }
    return fonts[f];
}

static void release_fonts(void)
{
    int i;
    for (i = 0; i < FONT_COUNT; i++)
        if (fonts[i]) {
            ID3DXFont_Release(fonts[i]);
            fonts[i] = NULL;
        }
}

int overlay_line_height(int f)
{
    return font(f) ? line_h[f] : 14;
}

static int hexval(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* The colour code {RRGGBB} at s, or -1. */
static int32_t color_code(const char *s)
{
    int32_t v = 0;
    int i, h;
    if (s[0] != '{')
        return -1;
    for (i = 1; i <= 6; i++) {
        if ((h = hexval(s[i])) < 0)
            return -1;
        v = v << 4 | h;
    }
    return s[7] == '}' ? v : -1;
}

/* The colour in force n characters into s. */
uint32_t overlay_color_at(const char *s, int n, uint32_t argb)
{
    int32_t code;
    int i;
    for (i = 0; i < n && s[i]; i++)
        if ((code = color_code(s + i)) >= 0) {
            argb = (argb & 0xFF000000) | (uint32_t)code;
            i += 7;
        }
    return argb;
}

static int extent(ID3DXFont *f, const char *s, int n)
{
    SIZE sz;
    return n > 0 && GetTextExtentPoint32A(ID3DXFont_GetDC(f), s, n, &sz) ? sz.cx : 0;
}

/* Width of the first n characters of s (n < 0: all of it), colour codes
 * taking none. */
int overlay_text_width(int f, const char *s, int n)
{
    int raw = f & TEXT_RAW, w = 0, run = 0, i;
    ID3DXFont *fo = font(f & 0xFF);
    if (!fo)
        return 0;
    for (i = 0; s[i] && (n < 0 || i < n); i++) {
        if (!raw && color_code(s + i) >= 0) {
            w += extent(fo, s + i - run, run);
            run = 0;
            i += 7;
            continue;
        }
        run++;
    }
    return w + extent(fo, s + i - run, run);
}

/* Draws s at x, y, cut off at x + max_w (max_w <= 0: not cut); returns the
 * width drawn. */
int overlay_text(int f, int x, int y, const char *s, uint32_t argb, int max_w)
{
    int raw = f & TEXT_RAW, x0 = x, right = max_w > 0 ? x + max_w : 0x7FFF, start = 0, i;
    ID3DXFont *fo = font(f &= 0xFF);
    if (!fo || !dev)
        return 0;
    rect_ready = 0;
    for (i = 0;; i++) {
        int32_t code = s[i] && !raw ? color_code(s + i) : -1;
        if (s[i] && code < 0)
            continue;
        if (i > start && x < right) {
            RECT rc = { x, y, right, y + line_h[f] };
            ID3DXFont_DrawTextA(fo, NULL, s + start, i - start, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE, argb);
            x += extent(fo, s + start, i - start);
        }
        if (!s[i])
            break;
        argb = (argb & 0xFF000000) | (uint32_t)code;
        i += 7;
        start = i + 1;
    }
    return (x < right ? x : right) - x0;
}

static void rect_states(void)
{
    IDirect3DDevice9_SetVertexShader(dev, NULL);
    IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    IDirect3DDevice9_SetTexture(dev, 0, NULL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    rect_ready = 1;
}

void overlay_rect(int x, int y, int w, int h, uint32_t argb)
{
    Vertex v[4];
    float l = x - 0.5f, t = y - 0.5f, r = l + w, b = t + h;
    if (!dev || w <= 0 || h <= 0 || !(argb >> 24))
        return;
    if (!rect_ready)
        rect_states();
    v[0] = (Vertex){ l, t, 0, 1, argb };
    v[1] = (Vertex){ r, t, 0, 1, argb };
    v[2] = (Vertex){ l, b, 0, 1, argb };
    v[3] = (Vertex){ r, b, 0, 1, argb };
    IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof *v);
}

void overlay_frame(int x, int y, int w, int h, uint32_t argb)
{
    overlay_rect(x, y, w, 1, argb);
    overlay_rect(x, y + h - 1, w, 1, argb);
    overlay_rect(x, y + 1, 1, h - 2, argb);
    overlay_rect(x + w - 1, y + 1, 1, h - 2, argb);
}

/* A one-line text editor's key: WM_CHAR types, WM_KEYDOWN moves the caret
 * and deletes. Returns EDIT_* bits. */
int edit_key(EditLine *e, UINT msg, WPARAM wp)
{
    int i;
    if (msg == WM_CHAR) {
        if (wp == '\r')
            return EDIT_HANDLED | EDIT_ENTER;
        if (wp == '\b') {
            if (!e->caret)
                return EDIT_HANDLED;
            for (i = --e->caret; i < e->len; i++)
                e->text[i] = e->text[i + 1];
            e->len--;
            return EDIT_HANDLED | EDIT_CHANGED;
        }
        if (wp < ' ' || wp > 0xFF || e->len >= (int)sizeof e->text - 1)
            return EDIT_HANDLED;
        for (i = ++e->len; i > e->caret; i--)
            e->text[i] = e->text[i - 1];
        e->text[e->caret++] = (char)wp;
        return EDIT_HANDLED | EDIT_CHANGED;
    }
    if (msg != WM_KEYDOWN)
        return msg == WM_KEYUP ? EDIT_HANDLED : 0;
    switch (wp) {
    case VK_LEFT:  if (e->caret) e->caret--; break;
    case VK_RIGHT: if (e->caret < e->len) e->caret++; break;
    case VK_HOME:  e->caret = 0; break;
    case VK_END:   e->caret = e->len; break;
    case VK_DELETE:
        if (e->caret == e->len)
            break;
        for (i = e->caret; i < e->len; i++)
            e->text[i] = e->text[i + 1];
        e->len--;
        return EDIT_HANDLED | EDIT_CHANGED;
    }
    return EDIT_HANDLED;
}

void edit_set(EditLine *e, const char *text)
{
    copy_string(e->text, text, sizeof e->text);
    e->len = e->caret = lstrlenA(e->text);
    e->scroll = 0;
}

/* Draws the line in w pixels, scrolled to show the caret; the caret
 * blinks when focused. */
void edit_draw(EditLine *e, int f, int x, int y, int w, uint32_t argb, int focused)
{
    int cx;
    f |= TEXT_RAW;
    if (e->caret < e->scroll)
        e->scroll = e->caret;
    while (e->scroll < e->caret && overlay_text_width(f, e->text + e->scroll, e->caret - e->scroll) > w - 2)
        e->scroll++;
    overlay_text(f, x, y, e->text + e->scroll, argb, w);
    if (focused && !(GetTickCount() / 500 & 1)) {
        cx = overlay_text_width(f, e->text + e->scroll, e->caret - e->scroll);
        overlay_rect(x + cx, y, 1, overlay_line_height(f & 0xFF), argb);
    }
}

static void read_size(void)
{
    IDirect3DSurface9 *bb;
    D3DSURFACE_DESC d;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)))
        return;
    if (SUCCEEDED(IDirect3DSurface9_GetDesc(bb, &d))) {
        width = d.Width;
        height = d.Height;
    }
    IDirect3DSurface9_Release(bb);
    if (height != font_basis) {
        release_fonts();
        font_basis = height;
    }
}

HWND overlay_window(void) { return window; }

static void to_pixels(POINT p, int *x, int *y)
{
    RECT rc;
    *x = *y = -1;
    if (!width && dev)
        read_size();
    if (!window || !GetClientRect(window, &rc) || rc.right <= 0 || rc.bottom <= 0)
        return;
    *x = p.x * width / rc.right;
    *y = p.y * height / rc.bottom;
}

/* The mouse in back buffer pixels. */
void overlay_cursor(int *x, int *y)
{
    POINT p;
    *x = *y = -1;
    if (window && GetCursorPos(&p) && ScreenToClient(window, &p))
        to_pixels(p, x, y);
}

/* A mouse message's point in back buffer pixels (the wheel's is on screen). */
void overlay_point(UINT msg, LPARAM lp, int *x, int *y)
{
    POINT p = { (short)LOWORD(lp), (short)HIWORD(lp) };
    if (msg == WM_MOUSEWHEEL && window)
        ScreenToClient(window, &p);
    to_pixels(p, x, y);
}

static void frame(void)
{
    if (!width)
        read_size();
    if (!width || !(dxut_visible() || console_visible()))
        return;
    if (!saved && FAILED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &saved))) {
        saved = NULL;
        return;
    }
    IDirect3DStateBlock9_Capture(saved);
    if (SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
        rect_ready = 0;
        dxut_render();
        console_render();
        IDirect3DDevice9_EndScene(dev);
    }
    IDirect3DStateBlock9_Apply(saved);
}

static HRESULT WINAPI hook_present(IDirect3DDevice9 *d, const RECT *src, const RECT *dst, HWND w, const RGNDATA *dirty)
{
    if (d == dev)
        frame();
    return orig_present(d, src, dst, w, dirty);
}

/* Reset needs every default-pool resource and state block let go first. */
static HRESULT WINAPI hook_reset(IDirect3DDevice9 *d, D3DPRESENT_PARAMETERS *pp)
{
    HRESULT hr;
    int i;
    if (d != dev)
        return orig_reset(d, pp);
    for (i = 0; i < FONT_COUNT; i++)
        if (fonts[i])
            ID3DXFont_OnLostDevice(fonts[i]);
    if (saved) {
        IDirect3DStateBlock9_Release(saved);
        saved = NULL;
    }
    hr = orig_reset(d, pp);
    for (i = 0; i < FONT_COUNT; i++)
        if (fonts[i] && SUCCEEDED(hr))
            ID3DXFont_OnResetDevice(fonts[i]);
    width = height = 0;
    return hr;
}

static void hook_device(IDirect3DDevice9 *d)
{
    void **vt = *(void ***)d, *p;
    orig_present = (PresentFn)vt[SLOT_PRESENT];
    orig_reset = (ResetFn)vt[SLOT_RESET];
    dev = d;
    p = (void *)hook_reset;
    ms_patch(&vt[SLOT_RESET], &p, sizeof p);
    p = (void *)hook_present;
    ms_patch(&vt[SLOT_PRESENT], &p, sizeof p);
    ms_log("overlay: drawing on device %08lX", (DWORD)d);
}

static LRESULT CALLBACK wndproc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (console_input(msg, wp, lp) || dxut_input(msg, wp, lp))
        return 0;
    return CallWindowProcA(orig_wndproc, w, msg, wp, lp);
}

/* SA-MP subclasses the game window once it is up; moonsamp's goes over
 * that, or over whatever is there after 10 s. */
static void hook_window(void)
{
    HWND *ps = *(HWND **)RS_GLOBAL_PS, w = ps ? *ps : NULL;
    HMODULE m = NULL;
    DWORD now = GetTickCount();
    if (!w)
        return;
    if (!wndproc_wait)
        wndproc_wait = now + 10000;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)GetWindowLongA(w, GWL_WNDPROC), &m);
    if (m != GetModuleHandleA("samp.dll") && (int)(now - wndproc_wait) < 0)
        return;
    window = w;
    orig_wndproc = (WNDPROC)SetWindowLongA(w, GWL_WNDPROC, (LONG)wndproc);
    ms_log("overlay: window input through %08lX", (DWORD)orig_wndproc);
}

/* Once a frame: hooks SA-MP's device and the game window once they exist. */
void overlay_tick(void)
{
    const Build *b = samp_build();
    IDirect3DDevice9 *d;
    if (!dev && b && (d = samp_global(b->device)))
        hook_device(d);
    if (!orig_wndproc)
        hook_window();
}
