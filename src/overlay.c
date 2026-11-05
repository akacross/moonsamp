/* What moonsamp draws in game: script dialogs (dxut.c) and the console
 * (console.c). They are drawn on SA-MP's real Direct3D device, in its
 * Present, so after SA-MP's own UI; and they see window input before SA-MP.
 *
 * Text takes SA-MP's colour codes: {RRGGBB} changes the colour, keeping the
 * alpha; not with TEXT_RAW. Coordinates are back buffer pixels. */
#define COBJMACROS
#include "moonsamp.h"
#include <d3d9.h>

typedef HRESULT (WINAPI *PresentFn)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef HRESULT (WINAPI *ResetFn)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);

typedef struct { float x, y, z, rhw; DWORD color; float u, v; } Vertex;

#define SLOT_RESET   16
#define SLOT_PRESENT 17
#define RS_GLOBAL_PS 0xC17054       /* RsGlobal.ps; its window first */
#define ATLAS_W      1024
#define PAD          2              /* glyph cells reach past the advance */
#define MAX_VERTS    (6 * 2048)

static const struct { const char *face; int size, weight; } spec[FONT_COUNT] = {
    { "Arial", 16, FW_BOLD },           /* FONT_UI, at 1080 lines */
    { "Lucida Console", 15, FW_NORMAL },
};

static IDirect3DDevice9 *dev;
static PresentFn orig_present;
static ResetFn orig_reset;
/* Every glyph of both fonts and a white block for rects, drawn by GDI once
 * per screen height into one managed texture, so a frame is one draw. */
static IDirect3DTexture9 *atlas;
static int atlas_h, atlas_failed, white_x, white_y;
static int line_h[FONT_COUNT], char_w[FONT_COUNT][256];
static short glyph_x[FONT_COUNT][256], glyph_y[FONT_COUNT][256];
static Vertex verts[MAX_VERTS];
static int vert_count, states_ready;
static IDirect3DStateBlock9 *saved;
static int width, height, font_basis;
static HWND window;
static WNDPROC orig_wndproc;
static DWORD wndproc_wait;

int overlay_width(void) { return width; }
int overlay_height(void) { return height; }

/* Creates the fonts and places their glyphs; returns the rows used. */
static int lay_out(HDC dc, HFONT *fonts)
{
    int f, c, x = 0, y = 0;
    TEXTMETRICA tm;
    for (f = 0; f < FONT_COUNT; f++) {
        int size = spec[f].size * height / 1080;
        if (size < 12)
            size = 12;
        fonts[f] = CreateFontA(size, 0, 0, 0, spec[f].weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, spec[f].face);
        if (!fonts[f])
            return 0;
        SelectObject(dc, fonts[f]);
        if (!GetTextMetricsA(dc, &tm) || !GetCharWidth32A(dc, 0, 255, char_w[f]))
            return 0;
        line_h[f] = tm.tmHeight;
        for (c = 0; c < 256; c++) {
            if (c < 32)
                char_w[f][c] = char_w[f][32];
            if (x + char_w[f][c] + 2 * PAD > ATLAS_W) {
                x = 0;
                y += line_h[f];
            }
            glyph_x[f][c] = (short)x;
            glyph_y[f][c] = (short)y;
            if (c >= 32)
                x += char_w[f][c] + 2 * PAD;
        }
        x = 0;
        y += line_h[f];
    }
    white_x = 0;
    white_y = y;
    return y + 4;
}

static void build_atlas(void)
{
    HFONT fonts[FONT_COUNT] = { 0 };
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bmp = NULL;
    uint32_t *bits = NULL;
    D3DLOCKED_RECT lr;
    int f, c, h = 0, y, x;
    atlas_failed = 1;
    if (!dc || !(h = lay_out(dc, fonts)))
        goto done;
    for (atlas_h = 64; atlas_h < h; atlas_h *= 2)
        ;
    {
        BITMAPINFO bi;
        memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = ATLAS_W;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    }
    if (!bmp || !bits)
        goto done;
    SelectObject(dc, bmp);
    memset(bits, 0, (size_t)ATLAS_W * h * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    for (f = 0; f < FONT_COUNT; f++) {
        SelectObject(dc, fonts[f]);
        for (c = 33; c < 256; c++) {
            char ch = (char)c;
            TextOutA(dc, glyph_x[f][c] + PAD, glyph_y[f][c], &ch, 1);
        }
    }
    GdiFlush();
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++)
            bits[(white_y + y) * ATLAS_W + white_x + x] = 0xFFFFFF;
    if (FAILED(IDirect3DDevice9_CreateTexture(dev, ATLAS_W, atlas_h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &atlas,
                                              NULL))) {
        atlas = NULL;
        goto done;
    }
    if (FAILED(IDirect3DTexture9_LockRect(atlas, 0, &lr, NULL, 0))) {
        IDirect3DTexture9_Release(atlas);
        atlas = NULL;
        goto done;
    }
    /* GDI's grey coverage becomes the alpha of white texels. */
    for (y = 0; y < atlas_h; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)lr.pBits + y * lr.Pitch);
        for (x = 0; x < ATLAS_W; x++) {
            uint32_t p = y < h ? bits[y * ATLAS_W + x] : 0, a = p & 0xFF;
            if ((p >> 8 & 0xFF) > a)
                a = p >> 8 & 0xFF;
            if ((p >> 16 & 0xFF) > a)
                a = p >> 16 & 0xFF;
            row[x] = a << 24 | 0xFFFFFF;
        }
    }
    IDirect3DTexture9_UnlockRect(atlas, 0);
    atlas_failed = 0;
done:
    if (atlas_failed)
        ms_log("overlay: could not draw the fonts, no text");
    if (dc)
        DeleteDC(dc);
    if (bmp)
        DeleteObject(bmp);
    for (f = 0; f < FONT_COUNT; f++)
        if (fonts[f])
            DeleteObject(fonts[f]);
}

static void release_atlas(void)
{
    if (atlas) {
        IDirect3DTexture9_Release(atlas);
        atlas = NULL;
    }
    atlas_failed = 0;
}

static int ready(void)
{
    if (!atlas && !atlas_failed && dev && height)
        build_atlas();
    return atlas != NULL;
}

int overlay_line_height(int f)
{
    return ready() ? line_h[f] : 14;
}

static void set_states(void)
{
    IDirect3DDevice9_SetVertexShader(dev, NULL);
    IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)atlas);
    IDirect3DDevice9_SetTexture(dev, 1, NULL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_BLENDOP, D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXCOORDINDEX, 0);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, FALSE);
    states_ready = 1;
}

static void flush(void)
{
    if (!vert_count)
        return;
    if (!states_ready)
        set_states();
    IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLELIST, vert_count / 3, verts, sizeof *verts);
    vert_count = 0;
}

/* A w x h quad at x, y showing the atlas from tx, ty. */
static void quad(int x, int y, int w, int h, int tx, int ty, int tw, int th, uint32_t argb)
{
    float l = x - 0.5f, t = y - 0.5f, r = l + w, b = t + h;
    float u0 = (float)tx / ATLAS_W, v0 = (float)ty / atlas_h;
    float u1 = (float)(tx + tw) / ATLAS_W, v1 = (float)(ty + th) / atlas_h;
    Vertex *v;
    if (vert_count + 6 > MAX_VERTS)
        flush();
    v = verts + vert_count;
    v[0] = (Vertex){ l, t, 0, 1, argb, u0, v0 };
    v[1] = (Vertex){ r, t, 0, 1, argb, u1, v0 };
    v[2] = (Vertex){ l, b, 0, 1, argb, u0, v1 };
    v[3] = v[1];
    v[4] = (Vertex){ r, b, 0, 1, argb, u1, v1 };
    v[5] = v[2];
    vert_count += 6;
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

/* Width of the first n characters of s (n < 0: all of it), colour codes
 * taking none. */
int overlay_text_width(int f, const char *s, int n)
{
    int raw = f & TEXT_RAW, w = 0, i;
    if (!ready())
        return 0;
    f &= 0xFF;
    for (i = 0; s[i] && (n < 0 || i < n); i++) {
        if (!raw && s[i] == '{' && color_code(s + i) >= 0)
            i += 7;
        else
            w += char_w[f][(uint8_t)s[i]];
    }
    return w;
}

/* Draws s at x, y, cut off at x + max_w (max_w <= 0: not cut); returns the
 * width drawn. */
int overlay_text(int f, int x, int y, const char *s, uint32_t argb, int max_w)
{
    int raw = f & TEXT_RAW, x0 = x, right = max_w > 0 ? x + max_w : 0x7FFF, i;
    int32_t code;
    if (!dev || !ready())
        return 0;
    f &= 0xFF;
    for (i = 0; s[i]; i++) {
        uint8_t c = (uint8_t)s[i];
        int w = char_w[f][c];
        if (!raw && c == '{' && (code = color_code(s + i)) >= 0) {
            argb = (argb & 0xFF000000) | (uint32_t)code;
            i += 7;
            continue;
        }
        if (x + w > right)
            break;
        if (c > ' ' && argb >> 24)
            quad(x - PAD, y, w + 2 * PAD, line_h[f], glyph_x[f][c], glyph_y[f][c], w + 2 * PAD, line_h[f], argb);
        x += w;
    }
    return x - x0;
}

void overlay_rect(int x, int y, int w, int h, uint32_t argb)
{
    if (!dev || w <= 0 || h <= 0 || !(argb >> 24) || !ready())
        return;
    quad(x, y, w, h, white_x + 1, white_y + 1, 2, 2, argb);
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
        release_atlas();
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
    /* Only what the overlay sets is saved and put back, and stream 0, which
     * DrawPrimitiveUP clears and RenderWare caches. */
    if (!saved) {
        if (FAILED(IDirect3DDevice9_BeginStateBlock(dev)))
            return;
        set_states();
        IDirect3DDevice9_SetStreamSource(dev, 0, NULL, 0, 0);
        if (FAILED(IDirect3DDevice9_EndStateBlock(dev, &saved))) {
            saved = NULL;
            return;
        }
    }
    IDirect3DStateBlock9_Capture(saved);
    if (SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
        states_ready = 0;
        dxut_render();
        console_render();
        flush();
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

/* Reset needs every state block let go first; the atlas is managed and
 * survives it. */
static HRESULT WINAPI hook_reset(IDirect3DDevice9 *d, D3DPRESENT_PARAMETERS *pp)
{
    HRESULT hr;
    if (d != dev)
        return orig_reset(d, pp);
    if (saved) {
        IDirect3DStateBlock9_Release(saved);
        saved = NULL;
    }
    hr = orig_reset(d, pp);
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
