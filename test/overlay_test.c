/* moonsamp's overlay under Wine (see run.sh): script dialogs (0B80-0BAB),
 * the console and its moonloader.log feed, drawn on a real Direct3D 9
 * device. samp.dll is mapped without running; its device global holds the
 * test's device and its game, input and dialog globals fake objects. Input
 * goes in as window messages through the game window's procedure, and each
 * frame is saved as a .bmp for a look. */
#define COBJMACROS
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <d3d9.h>
#include "../src/moonsamp.h"
#include "../src/builds.h"

typedef int8_t (THISCALL *Handler)(void *, int);

static int failed, passed;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (ok) passed++; else failed++;
}

/* ---- MoonLoader emulation (see raknet_test.c) ---------------------------- */

static uint8_t script[0x100], opbuf[512];
#define LOCAL(i) (*(int32_t *)(script + 0x3C + 4 * (i)))
#define COND     (script[0xC5])
static size_t n;
static void begin(void) { n = 0; }
static void put(const void *p, size_t len) { memcpy(opbuf + n, p, len); n += len; }
static void put_int(int32_t v) { opbuf[n++] = 0x01; put(&v, 4); }
static void put_local(int i) { uint16_t x = (uint16_t)i; opbuf[n++] = 0x03; put(&x, 2); }
static void put_str(const char *s)
{
    size_t len = strlen(s);
    opbuf[n++] = 0x0E;
    opbuf[n++] = (uint8_t)len;
    put(s, len);
}
static void op(int code)
{
    Handler *table = *(Handler **)0x469FEE;
    *(uint8_t **)(script + 0x10) = *(uint8_t **)(script + 0x14) = opbuf;
    script[0xD2] = 0;
    COND = 0xAA;
    table[code / 100](script, code);
}

static int32_t dlg;
static void add(int code, int id, const char *text, int x, int y, int w, int h)
{
    begin(); put_int(dlg); put_int(id);
    if (text) put_str(text);
    put_int(x); put_int(y); put_int(w); put_int(h);
    if (code == 0x0B96) put_int(10);
    op(code);
}
static int cond1(int code, int32_t a) { begin(); put_int(a); op(code); return COND == 1; }
static void set1(int code, int32_t v) { begin(); put_int(dlg); put_int(v); op(code); }
static int cond2(int code, int id) { begin(); put_int(dlg); put_int(id); op(code); return COND == 1; }
static void set2(int code, int id, int32_t v) { begin(); put_int(dlg); put_int(id); put_int(v); op(code); }
static int32_t get2(int code, int id) { begin(); put_int(dlg); put_int(id); put_local(0); op(code); return LOCAL(0); }
static char text[4096];
static const char *text2(int code, int id)
{
    memset(text, 0, sizeof text);
    begin(); put_int(dlg); put_int(id); put_int((int32_t)(uintptr_t)text); op(code);
    return text;
}
static int pop(int *event, int *id)
{
    begin(); put_int(dlg); put_local(0); put_local(1); op(0x0B81);
    *event = LOCAL(0);
    *id = LOCAL(1);
    return COND == 1;
}

/* ---- samp.dll stubs ------------------------------------------------------- */

static const Build *L;
static uint8_t game[0x100], input[0x1B00], dialog[0x100];
static int cursor_calls, input_enabling;
static void THISCALL s_cursor(void *g, int mode, int hide) { (void)hide; cursor_calls++; AT(g, L->game_cursor, int) = mode; }
static void THISCALL s_process_input(void *g) { (void)g; input_enabling++; }
static LPARAM game_keys[256];           /* last key message lParam per key, negated for up */
static LRESULT CALLBACK game_wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if ((m == WM_KEYDOWN || m == WM_KEYUP) && wp < 256)
        game_keys[wp] = m == WM_KEYDOWN ? lp : -lp;
    return DefWindowProcA(w, m, wp, lp);
}

static void jump(uintptr_t base, uint32_t rva, void *to)
{
    uint8_t *p = (uint8_t *)(base + rva), j[5] = { 0xE9 };
    DWORD old;
    *(int32_t *)(j + 1) = (int32_t)((uint8_t *)to - (p + 5));
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(p, j, 5);
}

static void set_global(uintptr_t base, uint32_t rva, void *v)
{
    DWORD old;
    VirtualProtect((void *)(base + rva), 4, PAGE_READWRITE, &old);
    *(void **)(base + rva) = v;
}

/* ---- window, device, frames ------------------------------------------------- */

static HWND hwnd;
static IDirect3DDevice9 *dev;
#define W 800
#define H 600
#define CLEAR 0xFF3A5F7Fu

static void click(int x, int y)
{
    SendMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessageA(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}
static void key(int vk) { SendMessageA(hwnd, WM_KEYDOWN, vk, 0); SendMessageA(hwnd, WM_KEYUP, vk, 0); }
static void type(const char *s) { for (; *s; s++) SendMessageA(hwnd, WM_CHAR, (uint8_t)*s, 0); }

static int write_bmp(const char *path, const uint8_t *px, int pitch, int w, int h)
{
    BITMAPFILEHEADER fh = { 0x4D42, 0, 0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) };
    BITMAPINFOHEADER ih = { sizeof ih, w, -h, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
    FILE *f = fopen(path, "wb");
    int y;
    if (!f)
        return 0;
    fh.bfSize = fh.bfOffBits + w * h * 4;
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    for (y = 0; y < h; y++)
        fwrite(px + y * pitch, 4, w, f);
    fclose(f);
    return 1;
}

/* Present as moonsamp's hook passes it on: what came before it in the call
 * was the overlay. Draw calls are counted. */
typedef HRESULT (WINAPI *PresentFn)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef HRESULT (WINAPI *DrawUpFn)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, const void *, UINT);
static PresentFn real_present;
static DrawUpFn real_draw_up;
static LARGE_INTEGER passed_on;
static int draws;
static HRESULT WINAPI inner_present(IDirect3DDevice9 *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    QueryPerformanceCounter(&passed_on);
    return real_present(d, a, b, w, r);
}
static HRESULT WINAPI count_draw_up(IDirect3DDevice9 *d, D3DPRIMITIVETYPE t, UINT n, const void *v, UINT stride)
{
    draws++;
    return real_draw_up(d, t, n, v, stride);
}
static void patch_slot(void **slot, void *to)
{
    DWORD old;
    VirtualProtect(slot, sizeof *slot, PAGE_EXECUTE_READWRITE, &old);
    *slot = to;
    VirtualProtect(slot, sizeof *slot, old, &old);
}

/* The overlay's mean time in count frames, in ms; *per_frame: its draws. */
static double frame_ms(int count, int *per_frame)
{
    LARGE_INTEGER f, a;
    double total = 0;
    int i;
    QueryPerformanceFrequency(&f);
    draws = 0;
    for (i = 0; i < count; i++) {
        IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
        QueryPerformanceCounter(&a);
        IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
        total += (double)(passed_on.QuadPart - a.QuadPart);
    }
    *per_frame = draws / count;
    return total * 1000.0 / (double)f.QuadPart / count;
}

/* One frame through Present (and so the overlay), saved to path; returns
 * the pixel at x, y. */
static uint32_t frame(const char *path, int x, int y)
{
    IDirect3DSurface9 *bb = NULL, *sys = NULL;
    D3DLOCKED_RECT lr;
    uint32_t px = 0;
    IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
    IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) ||
        FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, NULL)) ||
        FAILED(IDirect3DDevice9_GetRenderTargetData(dev, bb, sys)) ||
        FAILED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
        printf("could not read the back buffer\n");
    } else {
        px = *(uint32_t *)((uint8_t *)lr.pBits + y * lr.Pitch + x * 4);
        write_bmp(path, lr.pBits, lr.Pitch, W, H);
        IDirect3DSurface9_UnlockRect(sys);
    }
    if (sys) IDirect3DSurface9_Release(sys);
    if (bb) IDirect3DSurface9_Release(bb);
    return px;
}

static int darker(uint32_t px) { return (px & 0xFF) < 0x50 && (px >> 8 & 0xFF) < 0x40; }

static int iat_slots(HMODULE m, void *target)
{
    uint8_t *b = (uint8_t *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(b +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    int count = 0;
    for (; imp->Name; imp++) {
        void **slot = (void **)(b + imp->FirstThunk);
        for (; *slot; slot++)
            count += *slot == target;
    }
    return count;
}

static int log_has(const char *needle)
{
    static char buf[1 << 16];
    FILE *f = fopen("moonsamp.log", "rb");
    size_t got = f ? fread(buf, 1, sizeof buf - 1, f) : 0;
    if (f) fclose(f);
    buf[got] = 0;
    return strstr(buf, needle) != NULL;
}

__declspec(dllexport) int run(void)
{
    typedef IDirect3D9 *(WINAPI *CreateFn)(UINT);
    typedef void (*WriteLogFn)(const char *);
    HMODULE samp, ml, d3d9;
    uintptr_t base;
    uint32_t entry, px;
    size_t i;
    void (*tick)(void);
    void *write_file = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "WriteFile");
    WriteLogFn ml_write;
    static HWND window_slot;
    void **vt, *orig_present;
    int ev, id;

    setvbuf(stdout, NULL, _IONBF, 0);
    ml = LoadLibraryA("MoonLoader.asi");
    ml_write = ml ? (WriteLogFn)(void *)GetProcAddress(ml, "ml_write") : NULL;
    check(ml_write != NULL, "the stand-in MoonLoader.asi loads");
    if (!ml_write)
        return 1;
    ml_write("[12:00:00.000001] (system)\tbefore: written before the hook\r\n");
    check(iat_slots(ml, write_file) == 1, "it imports WriteFile once");

    samp = LoadLibraryExA("samp.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
    check(samp != NULL, "samp.dll maps without running");
    if (!samp)
        return 1;
    base = (uintptr_t)samp;
    entry = *(uint32_t *)(base + *(uint32_t *)(base + 0x3C) + 0x28);
    for (i = 0; i < sizeof builds / sizeof builds[0]; i++)
        if (builds[i].entry == entry)
            L = &builds[i];
    if (!L) { printf("samp.dll build %x not covered\n", entry); return 1; }
    printf("build %s\n", L->name);
    jump(base, L->set_cursor_mode, s_cursor);
    jump(base, L->process_input, s_process_input);
    set_global(base, L->game, game);
    set_global(base, L->input, input);
    set_global(base, L->dialog, dialog);

    {   /* the game window: SA-MP's procedure (a samp.dll address) over it */
        WNDCLASSA wc = { 0 };
        RECT rc = { 0, 0, W, H };
        wc.lpfnWndProc = DefWindowProcA;
        wc.lpszClassName = "moonsamp_overlay_test";
        RegisterClassA(&wc);
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        hwnd = CreateWindowA(wc.lpszClassName, "overlay", WS_OVERLAPPEDWINDOW, 0, 0, rc.right - rc.left,
                             rc.bottom - rc.top, NULL, NULL, NULL, NULL);
        jump(base, L->update_players, game_wndproc);
        SetWindowLongA(hwnd, GWL_WNDPROC, (LONG)(base + L->update_players));
        window_slot = hwnd;
        *(HWND **)0xC17054 = &window_slot;
    }
    {
        D3DPRESENT_PARAMETERS pp = { 0 };
        CreateFn create;
        IDirect3D9 *d3d;
        d3d9 = LoadLibraryA("d3d9.dll");
        create = d3d9 ? (CreateFn)(void *)GetProcAddress(d3d9, "Direct3DCreate9") : NULL;
        d3d = create ? create(D3D_SDK_VERSION) : NULL;
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_COPY;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferWidth = W;
        pp.BackBufferHeight = H;
        pp.hDeviceWindow = hwnd;
        if (!d3d || FAILED(IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev)))
            dev = NULL;
        check(dev != NULL, "a Direct3D 9 device");
        if (!dev)
            return 1;
        set_global(base, L->device, dev);
    }
    vt = *(void ***)dev;
    real_present = (PresentFn)vt[17];
    real_draw_up = (DrawUpFn)vt[83];
    patch_slot(&vt[17], (void *)inner_present);
    patch_slot(&vt[83], (void *)count_draw_up);
    orig_present = vt[17];

    check(LoadLibraryA("moonsamp.asi") != NULL, "moonsamp.asi loads");
    {
        uint8_t *tramp = *(int32_t *)0x561932 + (uint8_t *)0x561936;
        tick = (void (*)(void))(tramp + 7 + *(int32_t *)(tramp + 3));
    }
    tick();
    check(vt[17] != orig_present, "the device's Present is hooked");
    check(GetWindowLongA(hwnd, GWL_WNDPROC) != (LONG)(base + L->update_players),
          "the window goes through moonsamp, over SA-MP's procedure");
    check(iat_slots(ml, write_file) == 0, "MoonLoader.asi's WriteFile is hooked");
    ml_write("[12:00:01.000002] (script)\tafter: written through the hook\r\n");
    ml_write("[12:00:01.000003] (debug)\tthis debug line is hidden\r\n");
    ml_write("[12:00:01.000004] (error)\tsplit ");
    ml_write("across two writes\r\n");

    /* ---- dialogs ---- */
    begin(); put_str("moonsamp dialog"); put_local(0); op(0x0B80);
    dlg = LOCAL(0);
    check(dlg != 0 && cond1(0x0BAB, dlg), "0B80 creates a dialog, 0BAB it exists");
    check(cond1(0x0B87, dlg) && cond1(0x0BA0, dlg) && !cond1(0x0BA2, dlg), "visible, captioned, not minimized");
    begin(); put_int(dlg); put_int(100); put_int(120); put_int(320); put_int(260); op(0x0B84);
    begin(); put_int(dlg); put_local(0); put_local(1); put_local(2); put_local(3); op(0x0B85);
    check(LOCAL(0) == 100 && LOCAL(1) == 120 && LOCAL(2) == 320 && LOCAL(3) == 260, "0B84 / 0B85 position and size");
    add(0x0B82, 1, "OK", 10, 10, 100, 30);
    add(0x0B83, 2, "Check me", 10, 50, 180, 22);
    add(0x0B88, 3, "abc", 10, 80, 200, 24);
    add(0x0B91, 4, "{FFFF00}Static", 120, 10, 190, 30);
    add(0x0B96, 5, NULL, 10, 115, 200, 20);
    add(0x0B99, 6, NULL, 10, 145, 300, 100);
    add(0x0B82, 1, "duplicate", 0, 0, 10, 10);
    check(strcmp(text2(0x0B89, 1), "OK") == 0, "0B89 a button's text; a second id 1 is not added");
    check(strcmp(text2(0x0B89, 3), "abc") == 0, "0B89 an edit box's text");
    begin(); put_int(dlg); put_int(1); put_str("Go"); op(0x0B94);
    check(strcmp(text2(0x0B89, 1), "Go") == 0, "0B94 sets a control's text");
    set2(0x0B9E, 2, 1);
    check(cond2(0x0B92, 2), "0B9E / 0B92 checkbox checked");
    set2(0x0B98, 5, 7);
    check(get2(0x0B97, 5) == 7, "0B98 / 0B97 slider value");
    set2(0x0B98, 5, 99);
    check(get2(0x0B97, 5) == 10, "the slider stops at its max");
    {
        static const struct { const char *t; int data, at; } items[] = {
            { "one", 11, -1 }, { "two", 22, -1 }, { "zero", 0, 0 }, { "three", 33, 99 }, { "four", 44, -1 },
            { "five", 55, -1 }, { "six", 66, -1 },
        };
        for (i = 0; i < sizeof items / sizeof items[0]; i++) {
            begin(); put_int(dlg); put_int(6); put_str(items[i].t); put_int(items[i].data); put_int(items[i].at); op(0x0B9A);
        }
    }
    begin(); put_int(dlg); put_int(6); put_local(0); put_local(1); op(0x0B9B);
    check(LOCAL(0) == -1 && LOCAL(1) == 7, "0B9A / 0B9B seven items, none selected");
    begin(); put_int(dlg); put_int(6); put_int(0); put_int((int32_t)(uintptr_t)text); put_local(1); op(0x0B9D);
    check(strcmp(text, "zero") == 0 && LOCAL(1) == 0, "0B9D item 0 was inserted at the front");
    begin(); put_int(dlg); put_int(6); put_int(3); put_int((int32_t)(uintptr_t)text); put_local(1); op(0x0B9D);
    check(strcmp(text, "three") == 0 && LOCAL(1) == 33, "an index past the end appends");
    begin(); put_int(dlg); put_int(6); put_int(0); op(0x0B9C);
    begin(); put_int(dlg); put_int(6); put_int(0); put_int((int32_t)(uintptr_t)text); put_local(1); op(0x0B9D);
    check(strcmp(text, "one") == 0 && LOCAL(1) == 11, "0B9C deletes item 0");
    set2(0x0B90, 4, 0);
    check(!cond2(0x0B95, 4), "0B90 / 0B95 control hidden");
    set2(0x0B90, 4, 1);
    begin(); put_int(dlg); put_int(4); put_int(140); put_int(30); op(0x0BA6);
    begin(); put_int(dlg); put_int(4); put_int(125); put_int(12); op(0x0BA8);
    begin(); put_int(dlg); put_int(4); put_local(0); put_local(1); op(0x0BA7);
    check(LOCAL(0) == 140 && LOCAL(1) == 30, "0BA6 / 0BA7 control size");
    begin(); put_int(dlg); put_int(4); put_local(0); put_local(1); op(0x0BA9);
    check(LOCAL(0) == 125 && LOCAL(1) == 12, "0BA8 / 0BA9 control position");
    begin(); put_int(dlg); put_int(0); op(0x0B9F);
    check(!cond1(0x0BA0, dlg), "0B9F / 0BA0 caption off");
    begin(); put_int(dlg); put_int(1); op(0x0B9F);
    begin(); put_int(dlg); put_int(0xC8202840); op(0x0B93);
    check(!pop(&ev, &id), "0B81 no events yet");
    {   /* a throwaway dialog */
        int32_t d2;
        begin(); put_str("gone"); put_local(0); op(0x0B80);
        d2 = LOCAL(0);
        begin(); put_int(d2); op(0x0BA4);
        check(!cond1(0x0BAB, d2), "0BA4 deletes a dialog, 0BAB no longer");
    }

    /* ---- mouse and keys: only while SA-MP's cursor is shown ---- */
    AT(game, L->game_cursor, int) = 0;
    click(115, 135);
    check(!pop(&ev, &id), "no cursor: a click does nothing");
    AT(game, L->game_cursor, int) = 3;
    tick();
    click(115, 135);
    check(pop(&ev, &id) && ev == 0x0101 && id == 1, "a click on the button: EVENT_BUTTON_CLICKED");
    click(115, 181);
    check(pop(&ev, &id) && ev == 0x0401 && id == 2 && !cond2(0x0B92, 2), "the checkbox: EVENT_CHECKBOX_CHANGED, unchecked");
    click(115, 210);
    type("x");
    check(pop(&ev, &id) && ev == 0x0602 && id == 3, "typing in the edit box: EVENT_EDITBOX_CHANGE");
    check(strcmp(text2(0x0B89, 3), "abcx") == 0, "its text is abcx");
    type("\r");
    check(pop(&ev, &id) && ev == 0x0601 && id == 3, "Enter: EVENT_EDITBOX_STRING");
    SendMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(111, 245));
    SendMessageA(hwnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(315, 245));
    SendMessageA(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(315, 245));
    while (pop(&ev, &id) && ev == 0x0501)
        ;
    check(get2(0x0B97, 5) == 10, "dragging the slider to the right: 10");
    click(130, 268);
    begin(); put_int(dlg); put_int(6); put_local(0); put_local(1); op(0x0B9B);
    check(pop(&ev, &id) && ev == 0x0702 && id == 6 && LOCAL(0) >= 0, "a click on a list item: EVENT_LISTBOX_SELECTION");
    click(130, 268);
    check(pop(&ev, &id) && ev == 0x0701 && id == 6, "a second click: EVENT_LISTBOX_ITEM_DBLCLK");
    key(VK_DOWN);
    check(pop(&ev, &id) && ev == 0x0702, "Down in the focused list selects the next item");
    click(200, 110);
    check(cond1(0x0BA2, dlg), "a click on the caption minimizes");
    click(200, 110);
    check(!cond1(0x0BA2, dlg), "another restores");
    SendMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(200, 110));
    SendMessageA(hwnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(230, 120));
    SendMessageA(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(230, 120));
    begin(); put_int(dlg); put_local(0); put_local(1); put_local(2); put_local(3); op(0x0B85);
    check(LOCAL(0) == 130 && LOCAL(1) == 130 && !cond1(0x0BA2, dlg), "dragging the caption moves it");
    click(5, 5);
    type("q");
    check(!pop(&ev, &id), "a click outside takes the focus away");
    check(darker(frame("overlay_dialog.bmp", 420, 380)), "a frame: the dialog darkens the scene");

    /* ---- console ---- */
    AT(game, L->game_cursor, int) = 0;
    AT(input, 0x14E0, int) = 1;
    key(VK_OEM_3);
    check(!cond1(0x0C7E, 0), "~ while typing in the chat does not open the console");
    AT(input, 0x14E0, int) = 0;
    cursor_calls = 0;
    SendMessageA(hwnd, WM_KEYDOWN, 'W', 0x00110001);
    check(game_keys['W'] == 0x00110001, "the game sees W held");
    key(VK_OEM_3);
    SendMessageA(hwnd, WM_CHAR, '`', 0);
    check(cond1(0x0C7E, 0), "~ opens the console (0C7E)");
    check(game_keys['W'] == -(LPARAM)0xC0110001, "opening it releases W in the game");
    SendMessageA(hwnd, WM_KEYDOWN, 'W', 0x40110001);
    check(game_keys['W'] == -(LPARAM)0xC0110001, "and keeps W from the game while open");
    check(cursor_calls == 1 && AT(game, L->game_cursor, int) == 3, "it shows SA-MP's cursor");
    begin(); put_str("{66CCFF}sampfuncsLog line"); opbuf[n++] = 0; op(0x0B78);
    begin(); put_str("{FFFF00}[ML] {C0C0C0}scriptHelper's copy"); opbuf[n++] = 0; op(0x0B78);
    begin(); put_str("moontest"); put_int(0); op(0x0C63);
    begin(); put_str("moontest"); put_str("a test command"); op(0x0C80);
    type("nope\r");
    tick();
    check(log_has("console: unknown command \"nope\""), "a typed line runs in the tick: unknown command");
    type("help\r");
    tick();
    check(darker(frame("overlay_console.bmp", 20, 20)), "a frame: the console darkens the scene");
    key(VK_ESCAPE);
    SendMessageA(hwnd, WM_CHAR, 0x1B, 0);
    check(!cond1(0x0C7E, 0) && AT(game, L->game_cursor, int) == 0, "Esc closes it and hides the cursor again");
    key(VK_OEM_3);
    SendMessageA(hwnd, WM_CHAR, '`', 0);
    {
        char line[200];
        for (i = 0; i < 40; i++) {
            sprintf(line, "{A0A0A0}filler line %d: a long line that wraps across the width of the console window "
                          "{FF8080}in two colours, to show the wrap keeps them", (int)i);
            begin(); put_str(line); opbuf[n++] = 0; op(0x0B78);
        }
    }
    type("moo");
    key(VK_TAB);
    SendMessageA(hwnd, WM_CHAR, '\t', 0);

    /* ---- frames ---- */
    px = frame("overlay.bmp", 787, 20);
    check(px && darker(px), "a frame with both");
    {
        char line[240];
        double open_ms, closed_ms;
        int open_draws, closed_draws;
        for (i = 0; i < 1000; i++) {
            sprintf(line, "{A0A0A0}[12:00:00.%06d] (script)\tscroll line %d: {FFFFFF}long enough to wrap "
                          "across the console {FF8080}in three colours, as real log lines do", (int)i, (int)i);
            begin(); put_str(line); opbuf[n++] = 0; op(0x0B78);
        }
        open_ms = frame_ms(300, &open_draws);
        key(VK_OEM_3);
        SendMessageA(hwnd, WM_CHAR, '`', 0);
        set1(0x0B86, 0);
        closed_ms = frame_ms(300, &closed_draws);
        set1(0x0B86, 1);
        key(VK_OEM_3);
        SendMessageA(hwnd, WM_CHAR, '`', 0);
        printf("the overlay takes %.3f ms a frame with the console and a dialog open, %.3f ms with neither\n",
               open_ms, closed_ms);
        check(open_draws == 1 && closed_draws == 0, "the console and a dialog: one draw a frame, none when closed");
        /* Wine's own d3d9 draws in software here, which a frame waits on. */
        if (GetEnvironmentVariableA("D3D9", NULL, 0))
            check(open_ms < 0.5, "the console and a dialog take under 0.5 ms a frame");
    }
    {
        IDirect3DVertexBuffer9 *vb = NULL, *got = NULL;
        UINT off = 1, stride = 0;
        DWORD blend = 1, src = 0, fvf = 0, op = 0, filter = 0;
        int d;
        IDirect3DDevice9_CreateVertexBuffer(dev, 64, 0, 0, D3DPOOL_MANAGED, &vb, NULL);
        IDirect3DDevice9_SetStreamSource(dev, 0, vb, 0, 16);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_ONE);
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZ);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        frame_ms(1, &d);
        IDirect3DDevice9_GetStreamSource(dev, 0, &got, &off, &stride);
        IDirect3DDevice9_GetRenderState(dev, D3DRS_ALPHABLENDENABLE, &blend);
        IDirect3DDevice9_GetRenderState(dev, D3DRS_SRCBLEND, &src);
        IDirect3DDevice9_GetFVF(dev, &fvf);
        IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLOROP, &op);
        IDirect3DDevice9_GetSamplerState(dev, 0, D3DSAMP_MAGFILTER, &filter);
        check(d == 1 && got == vb && off == 0 && stride == 16 && !blend && src == D3DBLEND_ONE && fvf == D3DFVF_XYZ &&
              op == D3DTOP_SELECTARG2 && filter == D3DTEXF_LINEAR,
              "a frame puts the game's states back, stream 0 too");
        if (got)
            IDirect3DVertexBuffer9_Release(got);
        IDirect3DDevice9_SetStreamSource(dev, 0, NULL, 0, 0);
        if (vb)
            IDirect3DVertexBuffer9_Release(vb);
    }
    {
        D3DPRESENT_PARAMETERS pp = { 0 };
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_COPY;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferWidth = W;
        pp.BackBufferHeight = H;
        pp.hDeviceWindow = hwnd;
        check(IDirect3DDevice9_Reset(dev, &pp) == D3D_OK, "Reset succeeds: the overlay holds no default-pool resources");
    }
    check(darker(frame("overlay_reset.bmp", 20, 20)), "it draws again after the Reset");
    key(VK_ESCAPE);
    begin(); put_int(dlg); put_int(0); op(0x0B86);
    check(frame("overlay_off.bmp", 20, 20) == (CLEAR & 0xFFFFFF) || frame("overlay_off.bmp", 20, 20) == CLEAR,
          "nothing visible: the frame is the game's own");

    printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
