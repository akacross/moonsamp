/* SAMPFUNCS' render opcodes for CLEO scripts: lines, boxes, polygons, text,
 * textures and raw primitives. SAMPFUNCS draws when the opcode runs; a CLEO
 * script runs outside the game's scene, so here every draw is queued and the
 * overlay replays the frame's queue in Present (overlay.c), then clears it.
 * Coordinates are back buffer pixels, colours ARGB, arguments read raw as
 * SAMPFUNCS reads them (floats where its API takes floats).
 *
 * A texture handle points at a struct laid out like SAMPFUNCS'
 * (stTextureInfo: the texture at +0, the sprite at +4, width and height at
 * +8 and +0xC), so scripts reading those fields keep working. No sprite is
 * made: an ID3DXSprite left alive across a device Reset makes it fail. */
#define COBJMACROS
#include "moonsamp.h"
#include <d3d9.h>

IDirect3DDevice9 *overlay_device(void);

typedef struct { float x, y, z, rhw; DWORD color; float u, v; } Vtx;

#define FVF          (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
#define MAX_VTX      16384
#define MAX_CMDS     2048
#define MAX_PRIM     1024
#define FIRST_CHAR   32
#define ATLAS_W      1024
#define PI           3.14159265f

typedef struct {
    IDirect3DTexture9 *tex;     /* +0, as stTextureInfo */
    void *sprite;               /* +4: an ID3DXSprite when D3DX made one */
    UINT width, height;         /* +8, +0xC: the image's own size */
} Texture;

typedef struct {
    IDirect3DTexture9 *atlas;
    char name[32];
    int size, flags, line_h, atlas_h;
    unsigned char adv[256];
    short gx[256], gy[256];
    unsigned char gw[256];
} Font;

enum { CMD_DRAW, CMD_STATE };
typedef struct {
    uint8_t kind;
    D3DPRIMITIVETYPE prim;
    IDirect3DTexture9 *tex;
    int first, count;           /* vertices */
    DWORD state, value;
} Cmd;

static Vtx vtx[MAX_VTX];
static Cmd cmds[MAX_CMDS];
static int vtx_count, cmd_count;

/* begin/vertex/end (0C3B-0C40) */
static struct { int open; D3DPRIMITIVETYPE prim; DWORD color; float u, v; IDirect3DTexture9 *tex;
                Vtx v_buf[MAX_PRIM]; int n; } prim;

static float f_sin(float x) { __asm__("fsin" : "+t"(x)); return x; }
static float f_cos(float x) { __asm__("fcos" : "+t"(x)); return x; }
static float f_sqrt(float x) { __asm__("fsqrt" : "+t"(x)); return x; }

/* ---- the queue ------------------------------------------------------------ */

static Vtx *reserve(int n)
{
    if (vtx_count + n > MAX_VTX)
        return NULL;
    vtx_count += n;
    return vtx + vtx_count - n;
}

static void push_draw(D3DPRIMITIVETYPE p, IDirect3DTexture9 *tex, Vtx *first, int n)
{
    Cmd *c;
    if (!first || cmd_count >= MAX_CMDS)
        return;
    /* consecutive triangle lists with the same texture share one draw */
    if (p == D3DPT_TRIANGLELIST && cmd_count && cmds[cmd_count - 1].kind == CMD_DRAW &&
        cmds[cmd_count - 1].prim == p && cmds[cmd_count - 1].tex == tex &&
        cmds[cmd_count - 1].first + cmds[cmd_count - 1].count == (int)(first - vtx)) {
        cmds[cmd_count - 1].count += n;
        return;
    }
    c = &cmds[cmd_count++];
    c->kind = CMD_DRAW;
    c->prim = p;
    c->tex = tex;
    c->first = (int)(first - vtx);
    c->count = n;
}

static void set_v(Vtx *v, float x, float y, DWORD color, float u, float t)
{
    v->x = x - 0.5f, v->y = y - 0.5f, v->z = 0.0f, v->rhw = 1.0f;
    v->color = color, v->u = u, v->v = t;
}

/* A quad as two triangles, corners in order around it. */
static void quad4(IDirect3DTexture9 *tex, const float *xy, DWORD color, const float *uv)
{
    static const int order[6] = { 0, 1, 2, 0, 2, 3 };
    Vtx *v = reserve(6);
    int i;
    if (!v)
        return;
    for (i = 0; i < 6; i++) {
        int k = order[i];
        set_v(&v[i], xy[2 * k], xy[2 * k + 1], color, uv ? uv[2 * k] : 0.0f, uv ? uv[2 * k + 1] : 0.0f);
    }
    push_draw(D3DPT_TRIANGLELIST, tex, v, 6);
}

static void rect(float x, float y, float w, float h, DWORD color)
{
    float xy[8] = { x, y, x + w, y, x + w, y + h, x, y + h };
    if (w > 0 && h > 0)
        quad4(NULL, xy, color, NULL);
}

int sfrender_pending(void) { return cmd_count > 0; }

/* ---- replay, in the overlay's scene ---------------------------------------- */

static void stage(IDirect3DDevice9 *d, IDirect3DTexture9 *tex)
{
    IDirect3DDevice9_SetTexture(d, 0, (IDirect3DBaseTexture9 *)tex);
    IDirect3DDevice9_SetTextureStageState(d, 0, D3DTSS_COLOROP, tex ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
    IDirect3DDevice9_SetTextureStageState(d, 0, D3DTSS_ALPHAOP, tex ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
}

static UINT prim_count(D3DPRIMITIVETYPE p, int n)
{
    switch (p) {
    case D3DPT_POINTLIST:     return (UINT)n;
    case D3DPT_LINELIST:      return (UINT)(n / 2);
    case D3DPT_LINESTRIP:     return n > 1 ? (UINT)(n - 1) : 0;
    case D3DPT_TRIANGLELIST:  return (UINT)(n / 3);
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN:   return n > 2 ? (UINT)(n - 2) : 0;
    default:                  return 0;
    }
}

void sfrender_render(IDirect3DDevice9 *d)
{
    /* render states a script set are put back after its drawing */
    DWORD state_keys[32], state_vals[32];
    IDirect3DTexture9 *bound = (IDirect3DTexture9 *)-1;
    int nstates = 0, i, k;
    if (!cmd_count)
        return;
    IDirect3DDevice9_SetFVF(d, FVF);
    IDirect3DDevice9_SetSamplerState(d, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(d, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    for (i = 0; i < cmd_count; i++) {
        Cmd *c = &cmds[i];
        if (c->kind == CMD_STATE) {
            for (k = 0; k < nstates && state_keys[k] != c->state; k++)
                ;
            if (k == nstates && nstates < 32) {
                state_keys[nstates] = c->state;
                IDirect3DDevice9_GetRenderState(d, (D3DRENDERSTATETYPE)c->state, &state_vals[nstates]);
                nstates++;
            }
            IDirect3DDevice9_SetRenderState(d, (D3DRENDERSTATETYPE)c->state, c->value);
            continue;
        }
        if (c->tex != bound) {
            stage(d, c->tex);
            bound = c->tex;
        }
        if (prim_count(c->prim, c->count))
            IDirect3DDevice9_DrawPrimitiveUP(d, c->prim, prim_count(c->prim, c->count), vtx + c->first, sizeof(Vtx));
    }
    for (k = nstates - 1; k >= 0; k--)
        IDirect3DDevice9_SetRenderState(d, (D3DRENDERSTATETYPE)state_keys[k], state_vals[k]);
    stage(d, NULL);
    cmd_count = vtx_count = 0;
}

/* ---- fonts ------------------------------------------------------------------- */

/* Every glyph from 32 to 255, drawn by GDI white on black into one texture,
 * its brightness as alpha. The size is in points, as SAMPFUNCS' CD3DFont. */
static int build_font(Font *f)
{
    IDirect3DDevice9 *d = overlay_device();
    HDC dc;
    HFONT font, old;
    HBITMAP bmp;
    BITMAPINFO bi;
    TEXTMETRICA tm;
    uint32_t *bits = NULL;
    D3DLOCKED_RECT lr;
    int x = 0, y = 0, c, h;
    if (!d)
        return 0;
    dc = CreateCompatibleDC(NULL);
    h = -MulDiv(f->size, GetDeviceCaps(dc, LOGPIXELSY), 72);
    font = CreateFontA(h, 0, 0, 0, (f->flags & 1) ? FW_BOLD : FW_NORMAL, (f->flags & 2) != 0,
                       (f->flags & 0x10) != 0, (f->flags & 0x20) != 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, f->name);
    old = SelectObject(dc, font);
    GetTextMetricsA(dc, &tm);
    f->line_h = tm.tmHeight;
    /* lay out the cells, then size the texture to fit */
    for (c = FIRST_CHAR; c < 256; c++) {
        SIZE sz;
        char ch = (char)c;
        GetTextExtentPoint32A(dc, &ch, 1, &sz);
        f->adv[c] = (unsigned char)(sz.cx > 255 ? 255 : sz.cx);
        f->gw[c] = (unsigned char)(sz.cx + 2 > 255 ? 255 : sz.cx + 2);
        if (x + f->gw[c] > ATLAS_W)
            x = 0, y += f->line_h + 2;
        f->gx[c] = (short)x, f->gy[c] = (short)y;
        x += f->gw[c] + 1;
    }
    for (f->atlas_h = 64; f->atlas_h < y + f->line_h + 2; f->atlas_h *= 2)
        ;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = ATLAS_W;
    bi.bmiHeader.biHeight = -f->atlas_h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    if (bmp && bits) {
        HGDIOBJ old_bmp = SelectObject(dc, bmp);
        memset(bits, 0, (size_t)ATLAS_W * f->atlas_h * 4);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, TRANSPARENT);
        for (c = FIRST_CHAR; c < 256; c++) {
            char ch = (char)c;
            TextOutA(dc, f->gx[c] + 1, f->gy[c], &ch, 1);
        }
        GdiFlush();
        if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d, ATLAS_W, (UINT)f->atlas_h, 1, 0, D3DFMT_A8R8G8B8,
                                                     D3DPOOL_MANAGED, &f->atlas, NULL)) &&
            SUCCEEDED(IDirect3DTexture9_LockRect(f->atlas, 0, &lr, NULL, 0))) {
            int row, col;
            for (row = 0; row < f->atlas_h; row++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)lr.pBits + row * lr.Pitch);
                const uint32_t *src = bits + (size_t)row * ATLAS_W;
                for (col = 0; col < ATLAS_W; col++)
                    dst[col] = ((src[col] & 0xFF) << 24) | 0xFFFFFF;
            }
            IDirect3DTexture9_UnlockRect(f->atlas, 0);
        }
        SelectObject(dc, old_bmp);
        DeleteObject(bmp);
    }
    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);
    return f->atlas != NULL;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* {RRGGBB} at s: its colour, or -1. */
static int32_t color_tag(const char *s)
{
    int32_t v = 0;
    int i;
    if (s[0] != '{' || s[7] != '}')
        return -1;
    for (i = 1; i < 7; i++) {
        int h = hexval(s[i]);
        if (h < 0)
            return -1;
        v = v * 16 + h;
    }
    return v;
}

static int text_width(const Font *f, const char *s)
{
    int w = 0;
    for (; *s; s++) {
        if (color_tag(s) >= 0) {
            s += 7;
            continue;
        }
        if ((unsigned char)*s >= FIRST_CHAR)
            w += f->adv[(unsigned char)*s];
    }
    return w;
}

static void glyphs(const Font *f, const char *s, float x, float y, DWORD color, int plain)
{
    float tw = (float)ATLAS_W, th = (float)f->atlas_h;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        int32_t tag = color_tag(s);
        if (tag >= 0) {
            if (!plain)
                color = (color & 0xFF000000u) | (DWORD)tag;
            s += 7;
            continue;
        }
        if (c < FIRST_CHAR)
            continue;
        {
            float w = f->gw[c], h = (float)f->line_h;
            float xy[8] = { x - 1, y, x - 1 + w, y, x - 1 + w, y + h, x - 1, y + h };
            float u0 = f->gx[c] / tw, v0 = f->gy[c] / th, u1 = (f->gx[c] + w) / tw, v1 = (f->gy[c] + h) / th;
            float uv[8] = { u0, v0, u1, v0, u1, v1, u0, v1 };
            if (c != ' ')
                quad4(f->atlas, xy, color, uv);
        }
        x += f->adv[c];
    }
}

static void draw_text(Font *f, const char *s, float x, float y, DWORD color)
{
    DWORD shade = color & 0xFF000000u;
    if (!f->atlas && !build_font(f))
        return;
    if (f->flags & 4) {                     /* FCR_BORDER */
        glyphs(f, s, x - 1, y, shade, 1);
        glyphs(f, s, x + 1, y, shade, 1);
        glyphs(f, s, x, y - 1, shade, 1);
        glyphs(f, s, x, y + 1, shade, 1);
    } else if (f->flags & 8) {              /* FCR_SHADOW */
        glyphs(f, s, x + 1, y + 1, shade, 1);
    }
    glyphs(f, s, x, y, color, 0);
}

/* ---- D3DX, loaded on first use -------------------------------------------- */

typedef struct { UINT Width, Height, Depth, MipLevels; D3DFORMAT Format; D3DRESOURCETYPE ResourceType; int FileFormat; } ImageInfo;
typedef HRESULT (WINAPI *TexFromFileFn)(IDirect3DDevice9 *, LPCSTR, IDirect3DTexture9 **);
typedef HRESULT (WINAPI *TexFromMemFn)(IDirect3DDevice9 *, const void *, UINT, IDirect3DTexture9 **);
typedef HRESULT (WINAPI *InfoFromFileFn)(LPCSTR, ImageInfo *);
typedef HRESULT (WINAPI *InfoFromMemFn)(const void *, UINT, ImageInfo *);
static struct { int tried; TexFromFileFn from_file; TexFromMemFn from_mem; InfoFromFileFn info_file;
                InfoFromMemFn info_mem; } dx;

static void load_d3dx(void)
{
    static const char *names[] = { "d3dx9_43.dll", "d3dx9_42.dll", "d3dx9_41.dll", "d3dx9_40.dll",
                                   "d3dx9_39.dll", "d3dx9_38.dll", "d3dx9_37.dll", "d3dx9_36.dll",
                                   "d3dx9_35.dll", "d3dx9_34.dll", "d3dx9_33.dll", "d3dx9_32.dll",
                                   "d3dx9_31.dll", "d3dx9_30.dll", "d3dx9_29.dll", "d3dx9_28.dll",
                                   "d3dx9_27.dll", "d3dx9_26.dll", "d3dx9_25.dll", "d3dx9_24.dll" };
    size_t i;
    HMODULE m = NULL;
    if (dx.tried)
        return;
    dx.tried = 1;
    for (i = 0; i < sizeof names / sizeof names[0] && !m; i++)
        m = LoadLibraryA(names[i]);
    if (!m) {
        ms_log("render: no d3dx9 DLL, so textures cannot be loaded");
        return;
    }
    dx.from_file = (TexFromFileFn)(void *)GetProcAddress(m, "D3DXCreateTextureFromFileA");
    dx.from_mem = (TexFromMemFn)(void *)GetProcAddress(m, "D3DXCreateTextureFromFileInMemory");
    dx.info_file = (InfoFromFileFn)(void *)GetProcAddress(m, "D3DXGetImageInfoFromFileA");
    dx.info_mem = (InfoFromMemFn)(void *)GetProcAddress(m, "D3DXGetImageInfoFromFileInMemory");
}

static Texture *make_texture(IDirect3DTexture9 *tex, const ImageInfo *info)
{
    Texture *t = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *t);
    D3DSURFACE_DESC desc;
    if (!t) {
        IDirect3DTexture9_Release(tex);
        return NULL;
    }
    t->tex = tex;
    if (info && info->Width) {
        t->width = info->Width, t->height = info->Height;
    } else if (SUCCEEDED(IDirect3DTexture9_GetLevelDesc(tex, 0, &desc))) {
        t->width = desc.Width, t->height = desc.Height;
    }
    return t;
}

static void free_texture(Texture *t)
{
    if (!t)
        return;
    if (t->tex)
        IDirect3DTexture9_Release(t->tex);
    HeapFree(GetProcessHeap(), 0, t);
}

/* A texture or font a script may still draw this frame is only released
 * after it. */
static Texture *dead[64];
static Font *dead_fonts[64];
static int dead_count, dead_font_count;

static void free_font(Font *f)
{
    if (!f)
        return;
    if (f->atlas)
        IDirect3DTexture9_Release(f->atlas);
    HeapFree(GetProcessHeap(), 0, f);
}

/* ---- the opcodes --------------------------------------------------------------- */

static float pf(int i) { return ScriptParams[i].f; }
static DWORD pc(int i) { return (DWORD)ScriptParams[i].u; }

/* 0B68 render draw_line_point1 %1d% %2d% point2 %3d% %4d% width %5d% color %6d% */
static int8_t __stdcall op_0B68(Script *s)
{
    float x1, y1, x2, y2, w, dx_, dy, len;
    script_collect(s, 6);
    x1 = pf(0), y1 = pf(1), x2 = pf(2), y2 = pf(3), w = pf(4);
    dx_ = x2 - x1, dy = y2 - y1;
    len = f_sqrt(dx_ * dx_ + dy * dy);
    if (len > 0.0f && w > 0.0f) {
        float nx = -dy / len * w * 0.5f, ny = dx_ / len * w * 0.5f;
        float xy[8] = { x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny };
        quad4(NULL, xy, pc(5), NULL);
    }
    return 0;
}

/* 0B69 render draw_box_pos %1d% %2d% size %3d% %4d% color %5d% */
static int8_t __stdcall op_0B69(Script *s)
{
    script_collect(s, 5);
    rect(pf(0), pf(1), pf(2), pf(3), pc(4));
    return 0;
}

/* 0B6A render draw_box_with_border_pos %1d% %2d% size %3d% %4d% color %5d%
 * border_size %6d% border_color %7d%: the border a ring inside the box */
static int8_t __stdcall op_0B6A(Script *s)
{
    float x, y, w, h, b;
    DWORD fill, edge;
    script_collect(s, 7);
    x = pf(0), y = pf(1), w = pf(2), h = pf(3), fill = pc(4), b = pf(5), edge = pc(6);
    rect(x + b, y + b, w - 2 * b, h - 2 * b, fill);
    if (b > 0) {
        rect(x, y, w, b, edge);
        rect(x, y + h - b, w, b, edge);
        rect(x, y + b, b, h - 2 * b, edge);
        rect(x + w - b, y + b, b, h - 2 * b, edge);
    }
    return 0;
}

/* 0B6D render %4d% = create_font %1d% height %2d% flags %3d% */
static int8_t __stdcall op_0B6D(Script *s)
{
    char name[32];
    Font *f;
    script_string(s, name, sizeof name);
    script_collect(s, 2);
    f = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *f);
    if (f) {
        lstrcpynA(f->name, name, sizeof f->name);
        f->size = ScriptParams[0].i > 0 ? ScriptParams[0].i : 10;
        f->flags = ScriptParams[1].i;
        build_font(f);   /* now if the device is there, else on first use */
    }
    ScriptParams[0].p = f;
    script_store(s, 1);
    return 0;
}

/* 0B6E render release_font %1d% */
static int8_t __stdcall op_0B6E(Script *s)
{
    Font *f = (Font *)(uintptr_t)script_int(s);
    if (f && dead_font_count < 64)
        dead_fonts[dead_font_count++] = f;
    else
        free_font(f);
    script_cond(s, f != NULL);
    return 0;
}

/* 0B6B render %3d% = font %1d% draw_text %2d% length */
static int8_t __stdcall op_0B6B(Script *s)
{
    char text[256];
    Font *f = (Font *)(uintptr_t)script_int(s);
    script_string(s, text, sizeof text);
    if (f && !f->atlas)
        build_font(f);
    ScriptParams[0].i = f && f->atlas ? text_width(f, text) : 0;
    script_store(s, 1);
    return 0;
}

/* 0B6C render %2d% = font %1d% draw_height */
static int8_t __stdcall op_0B6C(Script *s)
{
    Font *f = (Font *)(uintptr_t)script_int(s);
    if (f && !f->atlas)
        build_font(f);
    ScriptParams[0].i = f && f->atlas ? f->line_h : 0;
    script_store(s, 1);
    return 0;
}

/* 0B6F render font %1d% draw_text %2d% pos %3d% %4d% color %5d% */
static int8_t __stdcall op_0B6F(Script *s)
{
    char text[512];
    Font *f = (Font *)(uintptr_t)script_int(s);
    script_string(s, text, sizeof text);
    script_collect(s, 3);
    if (f)
        draw_text(f, text, pf(0), pf(1), pc(2));
    return 0;
}

/* 0B70 render draw_polygon_pos %1d% %2d% size %3d% %4d% corners %5d% rotation %6d% color %7d% */
static int8_t __stdcall op_0B70(Script *s)
{
    float x, y, rx, ry, rot;
    int n, i;
    Vtx *v;
    script_collect(s, 7);
    x = pf(0), y = pf(1), rx = pf(2), ry = pf(3), n = ScriptParams[4].i, rot = pf(5) * PI / 180.0f;
    if (n < 3 || n > 256 || !(v = reserve(n * 3)))
        return 0;
    for (i = 0; i < n; i++) {
        float a0 = rot + 2 * PI * i / n, a1 = rot + 2 * PI * (i + 1) / n;
        set_v(&v[3 * i], x, y, pc(6), 0, 0);
        set_v(&v[3 * i + 1], x + f_cos(a0) * rx, y + f_sin(a0) * ry, pc(6), 0, 0);
        set_v(&v[3 * i + 2], x + f_cos(a1) * rx, y + f_sin(a1) * ry, pc(6), 0, 0);
    }
    push_draw(D3DPT_TRIANGLELIST, NULL, v, n * 3);
    return 0;
}

/* 0B71 render %2d% = load_texture_from_file %1d% */
static int8_t __stdcall op_0B71(Script *s)
{
    char path[MAX_PATH];
    IDirect3DDevice9 *d = overlay_device();
    IDirect3DTexture9 *tex = NULL;
    ImageInfo info;
    Texture *t = NULL;
    script_string(s, path, sizeof path);
    load_d3dx();
    memset(&info, 0, sizeof info);
    if (d && dx.from_file && SUCCEEDED(dx.from_file(d, path, &tex))) {
        if (dx.info_file)
            dx.info_file(path, &info);
        t = make_texture(tex, &info);
    }
    ScriptParams[0].p = t;
    script_store(s, 1);
    script_cond(s, t != NULL);
    return 0;
}

/* 0C8C render %3d% = load_texture_from_file_in_memory %1d% size %2d% */
static int8_t __stdcall op_0C8C(Script *s)
{
    IDirect3DDevice9 *d = overlay_device();
    IDirect3DTexture9 *tex = NULL;
    ImageInfo info;
    Texture *t = NULL;
    const void *data;
    UINT size;
    script_collect(s, 2);
    data = ScriptParams[0].p, size = ScriptParams[1].u;
    load_d3dx();
    memset(&info, 0, sizeof info);
    if (d && data && size && dx.from_mem && SUCCEEDED(dx.from_mem(d, data, size, &tex))) {
        if (dx.info_mem)
            dx.info_mem(data, size, &info);
        t = make_texture(tex, &info);
    }
    ScriptParams[0].p = t;
    script_store(s, 1);
    script_cond(s, t != NULL);
    return 0;
}

/* 0B72 render release_texture %1d% */
static int8_t __stdcall op_0B72(Script *s)
{
    Texture *t = (Texture *)(uintptr_t)script_int(s);
    if (t && dead_count < 64)
        dead[dead_count++] = t;
    else
        free_texture(t);
    script_cond(s, t != NULL);
    return 0;
}

/* 0B73 render draw_texture %1d% pos %2d% %3d% size %4d% %5d% rotation %6d% color %7d% */
static int8_t __stdcall op_0B73(Script *s)
{
    Texture *t;
    float x, y, w, h, a, c, sn, cx, cy;
    float xy[8], uv[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    int i;
    script_collect(s, 7);
    t = ScriptParams[0].p;
    x = pf(1), y = pf(2), w = pf(3), h = pf(4), a = pf(5) * PI / 180.0f;
    if (!t || !t->tex)
        return 0;
    c = f_cos(a), sn = f_sin(a), cx = x + w / 2, cy = y + h / 2;
    {
        float corner[8] = { x, y, x + w, y, x + w, y + h, x, y + h };
        for (i = 0; i < 4; i++) {
            float px = corner[2 * i] - cx, py = corner[2 * i + 1] - cy;
            xy[2 * i] = cx + px * c - py * sn;
            xy[2 * i + 1] = cy + px * sn + py * c;
        }
    }
    quad4(t->tex, xy, pc(6), uv);
    return 0;
}

/* 0C41 render %2d% = texture %1d% struct: its IDirect3DTexture9 */
static int8_t __stdcall op_0C41(Script *s)
{
    Texture *t = (Texture *)(uintptr_t)script_int(s);
    ScriptParams[0].p = t ? t->tex : NULL;
    script_store(s, 1);
    script_cond(s, t && t->tex);
    return 0;
}

/* 0C42 render %2d% = texture %1d% sprite: none is made (see the top) */
static int8_t __stdcall op_0C42(Script *s)
{
    Texture *t = (Texture *)(uintptr_t)script_int(s);
    ScriptParams[0].p = t ? t->sprite : NULL;
    script_store(s, 1);
    script_cond(s, t && t->sprite);
    return 0;
}

/* 0C43 render get_texture %1d% size_to %2d% %3d% */
static int8_t __stdcall op_0C43(Script *s)
{
    Texture *t = (Texture *)(uintptr_t)script_int(s);
    ScriptParams[0].u = t ? t->width : 0;
    ScriptParams[1].u = t ? t->height : 0;
    script_store(s, 2);
    script_cond(s, t != NULL);
    return 0;
}

/* 0C3B render begin %1d%: a D3DPRIMITIVETYPE */
static int8_t __stdcall op_0C3B(Script *s)
{
    prim.prim = (D3DPRIMITIVETYPE)script_int(s);
    prim.open = 1, prim.n = 0, prim.tex = NULL;
    prim.color = 0xFFFFFFFFu, prim.u = prim.v = 0.0f;
    return 0;
}

/* 0C3C render end */
static int8_t __stdcall op_0C3C(Script *s)
{
    Vtx *v;
    (void)s;
    if (prim.open && prim.n && (v = reserve(prim.n))) {
        memcpy(v, prim.v_buf, sizeof(Vtx) * (size_t)prim.n);
        push_draw(prim.prim, prim.tex, v, prim.n);
    }
    prim.open = 0;
    return 0;
}

/* 0C3D render color %1d% */
static int8_t __stdcall op_0C3D(Script *s)
{
    prim.color = (DWORD)script_int(s);
    return 0;
}

/* 0C3E render vertex %1d% %2d% */
static int8_t __stdcall op_0C3E(Script *s)
{
    script_collect(s, 2);
    if (prim.open && prim.n < MAX_PRIM)
        set_v(&prim.v_buf[prim.n++], pf(0), pf(1), prim.color, prim.u, prim.v);
    return 0;
}

/* 0C3F render set_tex_coord %1d% %2d% */
static int8_t __stdcall op_0C3F(Script *s)
{
    script_collect(s, 2);
    prim.u = pf(0), prim.v = pf(1);
    return 0;
}

/* 0C40 render bind_texture %1d%: for the primitive being built */
static int8_t __stdcall op_0C40(Script *s)
{
    Texture *t = (Texture *)(uintptr_t)script_int(s);
    prim.tex = t ? t->tex : NULL;
    script_cond(s, 1);
    return 0;
}

/* 0C44 render set_render_state %1d% = %2d%: until the end of this frame's drawing */
static int8_t __stdcall op_0C44(Script *s)
{
    script_collect(s, 2);
    if (cmd_count < MAX_CMDS) {
        Cmd *c = &cmds[cmd_count++];
        c->kind = CMD_STATE;
        c->state = ScriptParams[0].u;
        c->value = ScriptParams[1].u;
    }
    return 0;
}

/* Textures released last frame go once nothing queued can name them. */
static void free_dead(void)
{
    int i;
    if (cmd_count)
        return;
    for (i = 0; i < dead_count; i++)
        free_texture(dead[i]);
    for (i = 0; i < dead_font_count; i++)
        free_font(dead_fonts[i]);
    dead_count = dead_font_count = 0;
}

void sfrender_tick(void)
{
    free_dead();
}

void sfrender_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B68, op_0B68 }, { 0x0B69, op_0B69 }, { 0x0B6A, op_0B6A }, { 0x0B6B, op_0B6B },
        { 0x0B6C, op_0B6C }, { 0x0B6D, op_0B6D }, { 0x0B6E, op_0B6E }, { 0x0B6F, op_0B6F },
        { 0x0B70, op_0B70 }, { 0x0B71, op_0B71 }, { 0x0B72, op_0B72 }, { 0x0B73, op_0B73 },
        { 0x0C3B, op_0C3B }, { 0x0C3C, op_0C3C }, { 0x0C3D, op_0C3D }, { 0x0C3E, op_0C3E },
        { 0x0C3F, op_0C3F }, { 0x0C40, op_0C40 }, { 0x0C41, op_0C41 }, { 0x0C42, op_0C42 },
        { 0x0C43, op_0C43 }, { 0x0C44, op_0C44 }, { 0x0C8C, op_0C8C },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
