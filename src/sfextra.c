/* SAMPFUNCS opcodes MoonLoader never sends but CLEO scripts do: memory,
 * arrays, strings, math, line of sight, keys and the clipboard. Every other
 * SAMPFUNCS opcode moonsamp lacks gets a stub that steps over its
 * parameters, so a script using one goes on instead of jumping through a
 * null handler (a frozen game). */
#include "moonsamp.h"

#define RS_GLOBAL_PS 0xC17054       /* RsGlobal.ps; its window first */

typedef char (__cdecl *ScreenFn)(const float *, float *, float *, float *, char, char);
typedef char (__cdecl *SightFn)(const float *, const float *, void *, void **, char, char, char, char,
                                char, char, char, char);
typedef void (THISCALL *QuatSetFn)(void *, const void *);
typedef void (THISCALL *SetRotateQFn)(void *, const void *);
#define CalcScreenCoors      ((ScreenFn)0x70CE30)
#define ProcessLineOfSight   ((SightFn)0x56BA00)
#define CQuat_SetMatrix      ((QuatSetFn)0x59C3E0)
#define CMatrix_SetRotateQuat ((SetRotateQFn)0x59BBF0)
#define COLPOINT_SIZE        0x2C


static void store1(Script *s, int32_t v)
{
    ScriptParams[0].i = v;
    script_store(s, 1);
}

static void storef(Script *s, float f)
{
    ScriptParams[0].f = f;
    script_store(s, 1);
}

static void storep(Script *s, const void *p) { store1(s, (int32_t)(uintptr_t)p); }

static int readable(const void *p, size_t n) { return p && !IsBadReadPtr(p, (UINT_PTR)n); }
static int writable(void *p, size_t n)       { return p && !IsBadWritePtr(p, (UINT_PTR)n); }

static void write_mem(uint8_t *at, int32_t value, int size)
{
    if (size == 1 || size == 2 || size == 4)
        ms_patch(at, &value, (size_t)size);
}

static int32_t read_mem(const uint8_t *at, int size)
{
    if (size != 1 && size != 2 && size != 4)
        size = 4;
    if (!readable(at, (size_t)size))
        return 0;
    return size == 1 ? *at : size == 2 ? *(const uint16_t *)at : *(const int32_t *)at;
}

/* Strings that must outlive the opcode: an inline argument, a result. */
static char *ring(void)
{
    static char bufs[16][256];
    static int next;
    return bufs[next++ & 15];
}

/* A string argument as an address that stays good: inline text is copied. */
static const char *lasting(Script *s)
{
    char *buf = ring();
    return script_string(s, buf, 256);
}

/* Where a string output goes: the buffer an int variable or value points at
 * (as CLEO 4 has it), or a string variable's 8 or 16 bytes inline. cap is 0
 * when the size is unknown. */
typedef struct { char *p; size_t cap; } Dest;

static Dest dest_arg(Script *s)
{
    Dest d = { NULL, 0 };
    uint8_t type = *s->ip;
    uint16_t at;
    if (type == 0x02 || type == 0x03) {
        int32_t *v = script_var(s);
        d.p = v ? (char *)(uintptr_t)*v : NULL;
    } else if (type == 0x0A || type == 0x0B || type == 0x10 || type == 0x11) {
        memcpy(&at, s->ip + 1, 2);
        s->ip += 3;
        d.cap = type <= 0x0B ? 8 : 16;
        d.p = type == 0x0B || type == 0x11 ? (char *)&s->locals[at < 34 ? at : 0] : (char *)script_space() + at;
    } else if (type >= 0x01 && type <= 0x08) {
        script_collect(s, 1);
        d.p = ScriptParams[0].p;
    } else {
        char tmp[256];
        script_string(s, tmp, sizeof tmp);
    }
    return d;
}

static int dest_put(Dest d, const char *text)
{
    size_t n = (size_t)lstrlenA(text) + 1;
    if (d.cap && n > d.cap)
        n = d.cap;
    if (!writable(d.p, n))
        return 0;
    copy_string(d.p, text, n);
    return 1;
}

/* ---- memory ---------------------------------------------------------------- */

/* 0B2D samp write_memory offset %1d% value %2d% size %3d% */
/* SAMPFUNCS runs on 0.3.7-R1 only, so scripts give R1 offsets. The ones
 * scripts patch are moved to the running build's; on another build an
 * offset not known here is not written (it would land in other code). */
static const struct { uint32_t r1, in[7]; } samp_offsets[] = {
    /* the nametag drawer (shows nicknames above players), by build order:
     * R1 R2 R3 R3-1 R4 R5 DL */
    { 0x70D40, { 0x70D40, 0x70DE0, 0x74C30, 0x74C30, 0x75360, 0x75330, 0x74DC0 } },
};

static int build_index(void)
{
    static const char *const names[7] = {
        "0.3.7-R1", "0.3.7-R2", "0.3.7-R3", "0.3.7-R3-1", "0.3.7-R4", "0.3.7-R5", "0.3.DL-R1",
    };
    const Build *b = samp_build();
    int i;
    for (i = 0; b && i < 7; i++)
        if (lstrcmpA(b->name, names[i]) == 0)
            return i;
    return -1;
}

/* The running build's offset for an R1 one, or 0 when there is none. */
static uint32_t samp_offset(uint32_t r1)
{
    int b = build_index(), i;
    if (b == 0)
        return r1;
    for (i = 0; b > 0 && i < (int)(sizeof samp_offsets / sizeof samp_offsets[0]); i++)
        if (samp_offsets[i].r1 <= r1 && r1 < samp_offsets[i].r1 + 16)
            return samp_offsets[i].in[b] + (r1 - samp_offsets[i].r1);
    return 0;
}

static int8_t __stdcall op_0B2D(Script *s)
{
    static uint32_t refused;
    uint32_t off;
    script_collect(s, 3);
    if (!samp_base)
        return 0;
    if ((off = samp_offset(ScriptParams[0].u)) != 0) {
        write_mem((uint8_t *)(samp_base + off), ScriptParams[1].i, ScriptParams[2].i);
    } else if (refused != ScriptParams[0].u) {
        refused = ScriptParams[0].u;
        ms_log("0B2D: samp.dll+%X is an R1 offset with no match in this build; not written", refused);
    }
    return 0;
}

/* 0B2E %3d% = samp read_memory offset %1d% size %2d% */
static int8_t __stdcall op_0B2E(Script *s)
{
    script_collect(s, 2);
    {
        uint32_t off = samp_offset(ScriptParams[0].u);
        store1(s, samp_base && off ? read_mem((uint8_t *)(samp_base + off), ScriptParams[1].i) : 0);
    }
    return 0;
}

/* 0C0C read_memory_with_offset %1d% offset %2d% size %3d% store_to %4d% */
static int8_t __stdcall op_0C0C(Script *s)
{
    uint8_t *at;
    int size;
    script_collect(s, 3);
    at = (uint8_t *)ScriptParams[0].p + ScriptParams[1].i;
    size = ScriptParams[2].i;
    store1(s, ScriptParams[0].p ? read_mem(at, size) : 0);
    script_cond(s, ScriptParams[0].p != NULL);
    return 0;
}

/* 0C0D write_memory_with_offset %1d% offset %2d% size %3d% value %4d% */
static int8_t __stdcall op_0C0D(Script *s)
{
    script_collect(s, 4);
    if (ScriptParams[0].p)
        write_mem((uint8_t *)ScriptParams[0].p + ScriptParams[1].i, ScriptParams[3].i, ScriptParams[2].i);
    script_cond(s, ScriptParams[0].p != NULL);
    return 0;
}

/* 0C0E array %1d% element %2d% store_to %3d% */
static int8_t __stdcall op_0C0E(Script *s)
{
    int32_t *a;
    script_collect(s, 2);
    a = ScriptParams[0].p;
    store1(s, a && readable(a + ScriptParams[1].i, 4) ? a[ScriptParams[1].i] : 0);
    script_cond(s, a != NULL);
    return 0;
}

/* 0C0F array %1d% element %2d% value %3d% */
static int8_t __stdcall op_0C0F(Script *s)
{
    int32_t *a;
    script_collect(s, 3);
    a = ScriptParams[0].p;
    if (a && writable(a + ScriptParams[1].i, 4))
        a[ScriptParams[1].i] = ScriptParams[2].i;
    script_cond(s, a != NULL);
    return 0;
}

/* 0C10 memcpy %1d% %2d% size %3d% */
static int8_t __stdcall op_0C10(Script *s)
{
    uint8_t *d, *src;
    int n, i, ok;
    script_collect(s, 3);
    d = ScriptParams[0].p, src = ScriptParams[1].p, n = ScriptParams[2].i;
    ok = n > 0 && writable(d, (size_t)n) && readable(src, (size_t)n);
    if (ok) {
        if (d < src)
            for (i = 0; i < n; i++) d[i] = src[i];
        else
            for (i = n; i-- > 0;) d[i] = src[i];
    }
    script_cond(s, ok);
    return 0;
}

/* 0C11 memset %1d% value %2d% size %3d% */
static int8_t __stdcall op_0C11(Script *s)
{
    uint8_t *at;
    int i, n;
    script_collect(s, 3);
    at = ScriptParams[0].p;
    n = ScriptParams[2].i;
    if (n > 0 && writable(at, (size_t)n))
        for (i = 0; i < n; i++)
            at[i] = (uint8_t)ScriptParams[1].i;
    script_cond(s, at != NULL);
    return 0;
}

/* 0C12 memcmp %1d% %2d% size %3d%: true when equal */
static int8_t __stdcall op_0C12(Script *s)
{
    int n;
    script_collect(s, 3);
    n = ScriptParams[2].i;
    script_cond(s, n >= 0 && readable(ScriptParams[0].p, (size_t)n) && readable(ScriptParams[1].p, (size_t)n) &&
                       memcmp(ScriptParams[0].p, ScriptParams[1].p, (size_t)n) == 0);
    return 0;
}

/* 0C1D %4d% = array %1d% element %2d% el_size %3d% */
static int8_t __stdcall op_0C1D(Script *s)
{
    uint8_t *at;
    script_collect(s, 3);
    at = (uint8_t *)ScriptParams[0].p + ScriptParams[1].i * ScriptParams[2].i;
    store1(s, ScriptParams[0].p ? read_mem(at, ScriptParams[2].i) : 0);
    script_cond(s, ScriptParams[0].p != NULL);
    return 0;
}

/* 0C1E array %1d% element %2d% el_size %3d% = %4d% */
static int8_t __stdcall op_0C1E(Script *s)
{
    uint8_t *at;
    script_collect(s, 4);
    at = (uint8_t *)ScriptParams[0].p + ScriptParams[1].i * ScriptParams[2].i;
    if (ScriptParams[0].p && writable(at, (size_t)ScriptParams[2].i))
        write_mem(at, ScriptParams[3].i, ScriptParams[2].i);
    script_cond(s, ScriptParams[0].p != NULL);
    return 0;
}

/* 0C1F %4d% = string_array %1d% element %2d% size %3d%: the element's address */
static int8_t __stdcall op_0C1F(Script *s)
{
    script_collect(s, 3);
    storep(s, ScriptParams[0].p ? (uint8_t *)ScriptParams[0].p + ScriptParams[1].i * ScriptParams[2].i : NULL);
    return 0;
}

/* 0C20 string_array %1d% element %2d% size %3d% = %4d% */
static int8_t __stdcall op_0C20(Script *s)
{
    char buf[256];
    uint8_t *at;
    int size;
    const char *text;
    script_collect(s, 3);
    size = ScriptParams[2].i;
    at = ScriptParams[0].p ? (uint8_t *)ScriptParams[0].p + ScriptParams[1].i * size : NULL;
    text = script_string(s, buf, sizeof buf);
    if (size > 0 && writable(at, (size_t)size))
        copy_string((char *)at, text, (size_t)size);
    return 0;
}

/* 0BFA push_vars / 0BFB pop_vars: a script's 32 locals, 8 deep */
#define PUSH_SCRIPTS 32
static struct { Script *s; int depth; int32_t vars[8][32]; } pushed[PUSH_SCRIPTS];

static int8_t __stdcall op_0BFA(Script *s)
{
    int i, free_slot = -1;
    for (i = 0; i < PUSH_SCRIPTS; i++) {
        if (pushed[i].s == s)
            break;
        if (free_slot < 0 && (!pushed[i].s || !pushed[i].depth))
            free_slot = i;
    }
    if (i == PUSH_SCRIPTS) {
        if (free_slot < 0) {
            ms_log("0BFA push_vars: more than %d scripts holding pushed vars", PUSH_SCRIPTS);
            return 0;
        }
        i = free_slot;
        pushed[i].s = s;
        pushed[i].depth = 0;
    }
    if (pushed[i].depth < 8)
        memcpy(pushed[i].vars[pushed[i].depth++], s->locals, sizeof pushed[i].vars[0]);
    return 0;
}

static int8_t __stdcall op_0BFB(Script *s)
{
    int i;
    for (i = 0; i < PUSH_SCRIPTS; i++)
        if (pushed[i].s == s && pushed[i].depth) {
            memcpy(s->locals, pushed[i].vars[--pushed[i].depth], sizeof pushed[i].vars[0]);
            break;
        }
    return 0;
}

/* ---- strings ----------------------------------------------------------------- */

/* 0C13 strcpy %1d% %2d% */
static int8_t __stdcall op_0C13(Script *s)
{
    char buf[256];
    Dest d = dest_arg(s);
    dest_put(d, script_string(s, buf, sizeof buf));
    return 0;
}

/* 0C24 strncpy %1d% %2d% size %3d% */
static int8_t __stdcall op_0C24(Script *s)
{
    char buf[256];
    Dest d = dest_arg(s);
    const char *src = script_string(s, buf, sizeof buf);
    int n = script_int(s), i;
    if (d.cap && n > (int)d.cap)
        n = (int)d.cap;
    if (n > 0 && writable(d.p, (size_t)n)) {
        for (i = 0; i < n && src[i]; i++)
            d.p[i] = src[i];
        for (; i < n; i++)
            d.p[i] = 0;
    }
    return 0;
}

/* strcat (n < 0) and strncat */
static void cat(Dest d, const char *src, int n)
{
    int len, add, i;
    if (!d.p || !readable(d.p, 1))
        return;
    len = lstrlenA(d.p);
    add = lstrlenA(src);
    if (n >= 0 && add > n)
        add = n;
    if (d.cap && len + add + 1 > (int)d.cap)
        add = (int)d.cap - len - 1;
    if (add < 0 || !writable(d.p + len, (size_t)add + 1))
        return;
    for (i = 0; i < add; i++)
        d.p[len + i] = src[i];
    d.p[len + add] = 0;
}

/* 0C15 strcat %1d% %2d% */
static int8_t __stdcall op_0C15(Script *s)
{
    char buf[256];
    Dest d = dest_arg(s);
    cat(d, script_string(s, buf, sizeof buf), -1);
    return 0;
}

/* 0C27 strncat %1d% %2d% size %3d% */
static int8_t __stdcall op_0C27(Script *s)
{
    char buf[256];
    Dest d = dest_arg(s);
    const char *src = script_string(s, buf, sizeof buf);
    cat(d, src, script_int(s));
    return 0;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int compare(const char *a, const char *b, int n, int nocase)
{
    int i;
    for (i = 0; n < 0 || i < n; i++) {
        int x = (uint8_t)a[i], y = (uint8_t)b[i];
        if (nocase)
            x = lower(x), y = lower(y);
        if (x != y)
            return x - y;
        if (!x)
            break;
    }
    return 0;
}

/* 0C14 strcmp / 0C21 stricmp %1d% %2d%: true when equal */
static int8_t str_equal(Script *s, int nocase)
{
    char a[256], b[256];
    const char *x = script_string(s, a, sizeof a), *y = script_string(s, b, sizeof b);
    script_cond(s, compare(x, y, -1, nocase) == 0);
    return 0;
}
static int8_t __stdcall op_0C14(Script *s) { return str_equal(s, 0); }
static int8_t __stdcall op_0C21(Script *s) { return str_equal(s, 1); }

/* 0C25 strncmp %1d% %2d% size %3d% */
static int8_t __stdcall op_0C25(Script *s)
{
    char a[256], b[256];
    const char *x = script_string(s, a, sizeof a), *y = script_string(s, b, sizeof b);
    int n = script_int(s);
    script_cond(s, n <= 0 || compare(x, y, n, 0) == 0);
    return 0;
}

/* The first case-sensitive or case-insensitive match of sub in src. */
static const char *find(const char *src, const char *sub, int nocase)
{
    size_t n = (size_t)lstrlenA(sub), i;
    if (!n)
        return src;
    for (; *src; src++) {
        for (i = 0; i < n && src[i]; i++) {
            int c = (uint8_t)src[i], d = (uint8_t)sub[i];
            if (nocase)
                c = lower(c), d = lower(d);
            if (c != d)
                break;
        }
        if (i == n)
            return src;
    }
    return NULL;
}

/* 0C18 / 0C29 %3d% = strstr / stristr %1d% %2d%: the match's address in
 * the source the script gave (an inline source is kept, so it stays good). */
static int8_t search(Script *s, int nocase)
{
    char sub[256];
    const char *src = lasting(s);
    const char *hit = find(src, script_string(s, sub, sizeof sub), nocase);
    storep(s, hit);
    script_cond(s, hit != NULL);
    return 0;
}
static int8_t __stdcall op_0C18(Script *s) { return search(s, 0); }
static int8_t __stdcall op_0C29(Script *s) { return search(s, 1); }

static int in_set(int c, const char *set)
{
    for (; *set; set++)
        if ((uint8_t)*set == c)
            return 1;
    return 0;
}

/* 0C19 strcspn / 0C2E strspn: %3d% = the span of %1d% outside / inside %2d% */
static int8_t span(Script *s, int inside)
{
    char a[256], b[256];
    const char *x = script_string(s, a, sizeof a), *set = script_string(s, b, sizeof b);
    int n = 0;
    while (x[n] && in_set((uint8_t)x[n], set) == inside)
        n++;
    store1(s, n);
    script_cond(s, 1);
    return 0;
}
static int8_t __stdcall op_0C19(Script *s) { return span(s, 0); }
static int8_t __stdcall op_0C2E(Script *s) { return span(s, 1); }

/* 0C2B %3d% = strpbrk %1d% control %2d% */
static int8_t __stdcall op_0C2B(Script *s)
{
    char b[256];
    const char *x = lasting(s), *set = script_string(s, b, sizeof b);
    while (*x && !in_set((uint8_t)*x, set))
        x++;
    storep(s, *x ? x : NULL);
    script_cond(s, *x != 0);
    return 0;
}

/* 0C2A strchr / 0C2C strrchr: %3d% = the first / last %2d% in %1d% */
static int8_t find_char(Script *s, int last)
{
    const char *x = lasting(s), *hit = NULL;
    int c = script_int(s) & 0xFF;
    for (;; x++) {
        if ((uint8_t)*x == c) {
            hit = x;
            if (!last)
                break;
        }
        if (!*x)
            break;
    }
    storep(s, hit);
    script_cond(s, hit != NULL);
    return 0;
}
static int8_t __stdcall op_0C2A(Script *s) { return find_char(s, 0); }
static int8_t __stdcall op_0C2C(Script *s) { return find_char(s, 1); }

/* 0C16 %3d% = strtok %1d% %2d%: %1d% 0 goes on with the last string */
static int8_t __stdcall op_0C16(Script *s)
{
    static char own[256];
    static char *next;
    char b[256];
    const char *set;
    char *tok;
    uint8_t type = *s->ip;
    if (type == 0x02 || type == 0x03 || (type >= 0x01 && type <= 0x08)) {
        char *p = (char *)(uintptr_t)script_int(s);
        if (p)
            next = writable(p, 1) ? p : NULL;
    } else {
        script_string(s, own, sizeof own);
        next = own;
    }
    set = script_string(s, b, sizeof b);
    tok = next;
    if (tok) {
        while (*tok && in_set((uint8_t)*tok, set))
            tok++;
        if (!*tok) {
            tok = next = NULL;
        } else {
            next = tok;
            while (*next && !in_set((uint8_t)*next, set))
                next++;
            if (*next)
                *next++ = 0;
            else
                next = NULL;
        }
    }
    storep(s, tok);
    script_cond(s, tok != NULL);
    return 0;
}

/* 0C17 %2d% = strlen %1d% */
static int8_t __stdcall op_0C17(Script *s)
{
    char buf[256];
    store1(s, lstrlenA(script_string(s, buf, sizeof buf)));
    return 0;
}

/* in %1d% out %2d%: 0C26 strupr, 0C28 strlwr, 0C2D strrev */
enum { CASE_UPPER, CASE_LOWER, REVERSE };
static int8_t transform(Script *s, int how)
{
    char *buf = ring();
    const char *src = script_string(s, buf, 256);
    int n, i;
    if (src != buf)
        copy_string(buf, src, 256);
    n = lstrlenA(buf);
    for (i = 0; i < n; i++)
        if (how == CASE_UPPER && buf[i] >= 'a' && buf[i] <= 'z')
            buf[i] = (char)(buf[i] - 32);
        else if (how == CASE_LOWER)
            buf[i] = (char)lower((uint8_t)buf[i]);
    if (how == REVERSE)
        for (i = 0; i < n / 2; i++) {
            char t = buf[i];
            buf[i] = buf[n - 1 - i];
            buf[n - 1 - i] = t;
        }
    dest_put(dest_arg(s), buf);
    return 0;
}
static int8_t __stdcall op_0C26(Script *s) { return transform(s, CASE_UPPER); }
static int8_t __stdcall op_0C28(Script *s) { return transform(s, CASE_LOWER); }
static int8_t __stdcall op_0C2D(Script *s) { return transform(s, REVERSE); }

static int digit(int c, int radix)
{
    int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'z' ? c - 'a' + 10 :
            c >= 'A' && c <= 'Z' ? c - 'A' + 10 : 99;
    return v < radix ? v : -1;
}

static int32_t parse_int(const char *p, int radix, const char **end)
{
    uint32_t v = 0;
    int neg = 0, d;
    const char *start;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p == '+' || *p == '-')
        neg = *p++ == '-';
    if ((radix == 0 || radix == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && digit(p[2], 16) >= 0)
        p += 2, radix = 16;
    else if (radix == 0)
        radix = p[0] == '0' ? 8 : 10;
    if (radix < 2 || radix > 36)
        radix = 10;
    start = p;
    while ((d = digit((uint8_t)*p, radix)) >= 0)
        v = v * (uint32_t)radix + (uint32_t)d, p++;
    if (end)
        *end = p == start ? NULL : p;
    return neg ? -(int32_t)v : (int32_t)v;
}

static float parse_float(const char *p)
{
    double v = 0, scale = 1;
    int neg = 0, e = 0, eneg = 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '+' || *p == '-')
        neg = *p++ == '-';
    while (*p >= '0' && *p <= '9')
        v = v * 10 + (*p++ - '0');
    if (*p == '.')
        for (p++; *p >= '0' && *p <= '9'; p++)
            v += (*p - '0') * (scale /= 10);
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-')
            eneg = *p++ == '-';
        while (*p >= '0' && *p <= '9')
            e = e * 10 + (*p++ - '0');
        while (e-- > 0)
            v = eneg ? v / 10 : v * 10;
    }
    return (float)(neg ? -v : v);
}

/* 0C1A %2d% = atoi %1d% */
static int8_t __stdcall op_0C1A(Script *s)
{
    char buf[256];
    store1(s, parse_int(script_string(s, buf, sizeof buf), 10, NULL));
    return 0;
}

/* 0C1B %2d% = atof %1d% */
static int8_t __stdcall op_0C1B(Script *s)
{
    char buf[256];
    storef(s, parse_float(script_string(s, buf, sizeof buf)));
    return 0;
}

/* 0C2F %4d% = strtol %1d% radix %2d% store_end_to %3d% */
static int8_t __stdcall op_0C2F(Script *s)
{
    const char *x = lasting(s), *end;
    int32_t v = parse_int(x, script_int(s), &end), *end_var = script_var(s);
    if (end_var)
        *end_var = (int32_t)(uintptr_t)(end ? end : x);
    store1(s, v);
    return 0;
}

/* 0C1C %3d% = itoa %1d% radix %2d%: into the buffer %3d% points at */
static int8_t __stdcall op_0C1C(Script *s)
{
    char tmp[40], out[40];
    uint32_t v;
    int radix, neg, n = 0, i = 0;
    script_collect(s, 2);
    radix = ScriptParams[1].i;
    if (radix < 2 || radix > 36)
        radix = 10;
    neg = radix == 10 && ScriptParams[0].i < 0;
    v = neg ? 0u - ScriptParams[0].u : ScriptParams[0].u;
    do {
        int d = (int)(v % (uint32_t)radix);
        tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= (uint32_t)radix;
    } while (v);
    if (neg)
        out[i++] = '-';
    while (n)
        out[i++] = tmp[--n];
    out[i] = 0;
    dest_put(dest_arg(s), out);
    return 0;
}

/* 0C22 bin %1d% to_hex %2d% size %3d% */
static int8_t __stdcall op_0C22(Script *s)
{
    static const char hex[] = "0123456789ABCDEF";
    const uint8_t *src;
    char *dst;
    int n, i;
    script_collect(s, 3);
    src = ScriptParams[0].p, dst = ScriptParams[1].p, n = ScriptParams[2].i;
    if (n < 0 || !readable(src, (size_t)n) || !writable(dst, (size_t)n * 2 + 1)) {
        script_cond(s, 0);
        return 0;
    }
    for (i = 0; i < n; i++) {
        dst[i * 2] = hex[src[i] >> 4];
        dst[i * 2 + 1] = hex[src[i] & 15];
    }
    dst[n * 2] = 0;
    script_cond(s, 1);
    return 0;
}

/* 0C23 hex %1d% to_bin %2d% buf_size %3d% */
static int8_t __stdcall op_0C23(Script *s)
{
    char buf[256];
    const char *src = script_string(s, buf, sizeof buf);
    uint8_t *dst;
    int size, i = 0;
    script_collect(s, 2);
    dst = ScriptParams[0].p, size = ScriptParams[1].i;
    if (size < 0 || !writable(dst, (size_t)size)) {
        script_cond(s, 0);
        return 0;
    }
    while (i < size && digit((uint8_t)src[0], 16) >= 0 && digit((uint8_t)src[1], 16) >= 0) {
        dst[i++] = (uint8_t)(digit((uint8_t)src[0], 16) << 4 | digit((uint8_t)src[1], 16));
        src += 2;
    }
    script_cond(s, i > 0);
    return 0;
}

/* 0C3A %1d% = string %2d% pointer ...: the address of a string argument */
static int8_t __stdcall op_0C3A(Script *s)
{
    int32_t *out = script_var(s);
    uint8_t type = *s->ip;
    const char *p;
    uint16_t at;
    if (type == 0x0A || type == 0x0B || type == 0x10 || type == 0x11) {
        memcpy(&at, s->ip + 1, 2);
        s->ip += 3;
        p = type == 0x0B || type == 0x11 ? (const char *)&s->locals[at < 34 ? at : 0] : (const char *)script_space() + at;
    } else {
        p = lasting(s);
    }
    script_skip_varargs(s);
    if (out)
        *out = (int32_t)(uintptr_t)p;
    return 0;
}

/* ---- math -------------------------------------------------------------------- */

static float f_sqrt(float x) { __asm__("fsqrt" : "+t"(x)); return x; }
static float f_sin(float x)  { __asm__("fsin" : "+t"(x)); return x; }
static float f_cos(float x)  { __asm__("fcos" : "+t"(x)); return x; }
static float f_tan(float x)  { __asm__("fptan\n\tfstp %%st(0)" : "+t"(x)); return x; }
static float f_atan2(float y, float x)
{
    float r;
    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}
static float f_abs(float x) { return x < 0 ? -x : x; }

static float f_floor(float x)
{
    float t;
    if (f_abs(x) >= 8388608.0f || x != x)
        return x;
    t = (float)(int32_t)x;
    return t > x ? t - 1.0f : t;
}
static float f_ceil(float x) { return -f_floor(-x); }

/* 2^x for any x: f2xm1 takes |x| <= 1, fscale the integer part */
static float f_exp2(float x)
{
    float r;
    __asm__("fld %%st(0)\n\tfrndint\n\tfsubr %%st, %%st(1)\n\tfxch\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)"
            : "=t"(r) : "0"(x));
    return r;
}
static float f_log2(float x)
{
    float r;
    __asm__("fld1\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
    return r;
}
static float f_pow(float a, float b)
{
    float r;
    if (b == 0.0f)
        return 1.0f;
    if (a == 0.0f)
        return 0.0f;
    r = f_exp2(b * f_log2(f_abs(a)));
    if (a < 0) {
        if (f_floor(b) != b)
            return 0.0f;
        if ((int32_t)b & 1)
            r = -r;
    }
    return r;
}

#define RAD_TO_DEG 57.29577951f

/* 0C00-0C08, 0C0A, 0C0B math %2d% = f %1d%; angles in radians */
static int8_t math1(Script *s, int which)
{
    float x = script_float(s), r = 0;
    switch (which) {
    case 0x00: r = f_abs(x); break;
    case 0x01: r = x * RAD_TO_DEG; break;
    case 0x02: r = x / RAD_TO_DEG; break;
    case 0x03: r = f_sin(x); break;
    case 0x04: r = f_atan2(x, f_sqrt(1.0f - x * x)); break;
    case 0x05: r = f_cos(x); break;
    case 0x06: r = f_atan2(f_sqrt(1.0f - x * x), x); break;
    case 0x07: r = f_tan(x); break;
    case 0x08: r = f_atan2(x, 1.0f); break;
    case 0x0A: r = f_ceil(x); break;
    case 0x0B: r = f_floor(x); break;
    }
    storef(s, r);
    return 0;
}
static int8_t __stdcall op_0C00(Script *s) { return math1(s, 0x00); }
static int8_t __stdcall op_0C01(Script *s) { return math1(s, 0x01); }
static int8_t __stdcall op_0C02(Script *s) { return math1(s, 0x02); }
static int8_t __stdcall op_0C03(Script *s) { return math1(s, 0x03); }
static int8_t __stdcall op_0C04(Script *s) { return math1(s, 0x04); }
static int8_t __stdcall op_0C05(Script *s) { return math1(s, 0x05); }
static int8_t __stdcall op_0C06(Script *s) { return math1(s, 0x06); }
static int8_t __stdcall op_0C07(Script *s) { return math1(s, 0x07); }
static int8_t __stdcall op_0C08(Script *s) { return math1(s, 0x08); }
static int8_t __stdcall op_0C0A(Script *s) { return math1(s, 0x0A); }
static int8_t __stdcall op_0C0B(Script *s) { return math1(s, 0x0B); }

/* 0C09 math %3d% = power %1d% %2d% */
static int8_t __stdcall op_0C09(Script *s)
{
    script_collect(s, 2);
    storef(s, f_pow(ScriptParams[0].f, ScriptParams[1].f));
    return 0;
}

/* A CMatrix-sized scratch: right, forward, up at 0, 0x10, 0x20. */
static float scratch_matrix[18];

/* 0C30 math matrix %1d% to_quat %2d% (addresses) */
static int8_t __stdcall op_0C30(Script *s)
{
    script_collect(s, 2);
    if (readable(ScriptParams[0].p, 0x30) && writable(ScriptParams[1].p, 16))
        CQuat_SetMatrix(ScriptParams[1].p, ScriptParams[0].p);
    return 0;
}

/* 0C31 math quat %1d% to_matrix %2d%: the rotation only */
static int8_t __stdcall op_0C31(Script *s)
{
    script_collect(s, 2);
    if (readable(ScriptParams[0].p, 16) && writable(ScriptParams[1].p, 0x30))
        CMatrix_SetRotateQuat(ScriptParams[1].p, ScriptParams[0].p);
    return 0;
}

/* 0C32 math matrix right %1-3d% front %4-6d% up %7-9d% to_quat %10-13d% */
static int8_t __stdcall op_0C32(Script *s)
{
    float q[4];
    int i;
    script_collect(s, 9);
    memset(scratch_matrix, 0, sizeof scratch_matrix);
    for (i = 0; i < 3; i++) {
        scratch_matrix[i] = ScriptParams[i].f;
        scratch_matrix[4 + i] = ScriptParams[3 + i].f;
        scratch_matrix[8 + i] = ScriptParams[6 + i].f;
    }
    CQuat_SetMatrix(q, scratch_matrix);
    for (i = 0; i < 4; i++)
        ScriptParams[i].f = q[i];
    script_store(s, 4);
    return 0;
}

/* 0C33 math quat %1-4d% to_matrix right %5-7d% front %8-10d% up %11-13d% */
static int8_t __stdcall op_0C33(Script *s)
{
    float q[4];
    int i;
    script_collect(s, 4);
    for (i = 0; i < 4; i++)
        q[i] = ScriptParams[i].f;
    CMatrix_SetRotateQuat(scratch_matrix, q);
    for (i = 0; i < 3; i++) {
        ScriptParams[i].f = scratch_matrix[i];
        ScriptParams[3 + i].f = scratch_matrix[4 + i];
        ScriptParams[6 + i].f = scratch_matrix[8 + i];
    }
    script_store(s, 9);
    return 0;
}

/* 0B66 render argb %1d% to_a %2d% r %3d% g %4d% b %5d% */
static int8_t __stdcall op_0B66(Script *s)
{
    uint32_t c = (uint32_t)script_int(s);
    ScriptParams[0].i = (int32_t)(c >> 24);
    ScriptParams[1].i = (int32_t)(c >> 16 & 0xFF);
    ScriptParams[2].i = (int32_t)(c >> 8 & 0xFF);
    ScriptParams[3].i = (int32_t)(c & 0xFF);
    script_store(s, 4);
    return 0;
}

/* 0B67 render a %1d% r %2d% g %3d% b %4d% to_argb %5d% */
static int8_t __stdcall op_0B67(Script *s)
{
    script_collect(s, 4);
    store1(s, (int32_t)((ScriptParams[0].u & 0xFF) << 24 | (ScriptParams[1].u & 0xFF) << 16 |
                        (ScriptParams[2].u & 0xFF) << 8 | (ScriptParams[3].u & 0xFF)));
    return 0;
}

/* ---- world, input, system ------------------------------------------------------ */

/* 0B55 convert_3D_coords %1d% %2d% %3d% to_screen %4d% %5d%: window pixels,
 * true when in front of the camera */
static int8_t __stdcall op_0B55(Script *s)
{
    float in[3], out[3] = { 0, 0, 0 }, w, h;
    char ok;
    script_collect(s, 3);
    in[0] = ScriptParams[0].f, in[1] = ScriptParams[1].f, in[2] = ScriptParams[2].f;
    ok = CalcScreenCoors(in, out, &w, &h, 1, 1);
    ScriptParams[0].f = out[0];
    ScriptParams[1].f = out[1];
    script_store(s, 2);
    script_cond(s, ok != 0);
    return 0;
}

/* 0B5E get_cursor_pos %1d% %2d%: in the game window's client area */
static int8_t __stdcall op_0B5E(Script *s)
{
    HWND *ps = *(HWND **)RS_GLOBAL_PS;
    POINT p = { 0, 0 };
    GetCursorPos(&p);
    if (ps && *ps)
        ScreenToClient(*ps, &p);
    ScriptParams[0].i = p.x;
    ScriptParams[1].i = p.y;
    script_store(s, 2);
    return 0;
}

/* 0B56 set_game_key %1d% state %2d%: CPad 0's NewState, a short per key */
static int8_t __stdcall op_0B56(Script *s)
{
    script_collect(s, 2);
    if (ScriptParams[0].u < 24)
        ((int16_t *)0xB73458)[ScriptParams[0].u] = (int16_t)ScriptParams[1].i;
    return 0;
}

/* 0BFE %1d% = get_tick_count */
static int8_t __stdcall op_0BFE(Script *s)
{
    store1(s, (int32_t)GetTickCount());
    return 0;
}

/* 0BFF process_line_of_sight start %1-3d% end %4-6d% check_solid car actor
 * object particle see_through ignore_some_objects_for_camera %7-13d%
 * store_col_point_to %14d% entity_to %15d% */
static int8_t __stdcall op_0BFF(Script *s)
{
    uint8_t col[COLPOINT_SIZE];
    void *entity = NULL, *col_to;
    float a[3], b[3];
    char hit, f[7];
    int32_t *entity_var;
    int i;
    script_collect(s, 14);
    for (i = 0; i < 3; i++)
        a[i] = ScriptParams[i].f, b[i] = ScriptParams[3 + i].f;
    for (i = 0; i < 7; i++)
        f[i] = ScriptParams[6 + i].i != 0;
    col_to = ScriptParams[13].p;
    memset(col, 0, sizeof col);
    hit = ProcessLineOfSight(a, b, col, &entity, f[0], f[1], f[2], f[3], f[4], f[5], f[6], 0);
    if (writable(col_to, sizeof col))
        memcpy(col_to, col, sizeof col);
    entity_var = script_var(s);
    if (entity_var)
        *entity_var = (int32_t)(uintptr_t)entity;
    script_cond(s, hit != 0);
    return 0;
}

/* 0C89 key_just_pressed %1d% */
static int8_t __stdcall op_0C89(Script *s)
{
    int k = script_int(s);
    script_cond(s, (unsigned)k < 256 && cleo5_key_hit(k));
    return 0;
}

/* 0C67 get_windows_environment_variable %1d% to %2d% size %3d% */
static int8_t __stdcall op_0C67(Script *s)
{
    char name[256];
    DWORD n = 0;
    script_string(s, name, sizeof name);
    script_collect(s, 2);
    if (ScriptParams[1].i > 0 && writable(ScriptParams[0].p, (size_t)ScriptParams[1].i))
        n = GetEnvironmentVariableA(name, ScriptParams[0].p, (DWORD)ScriptParams[1].i);
    script_cond(s, n > 0 && n < (DWORD)ScriptParams[1].i);
    return 0;
}

/* 0C68 unicode_string %1d% to_ansi %2d% size %3d% (bytes) */
static int8_t __stdcall op_0C68(Script *s)
{
    int n = 0;
    script_collect(s, 3);
    if (ScriptParams[2].i > 0 && readable(ScriptParams[0].p, 2) && writable(ScriptParams[1].p, (size_t)ScriptParams[2].i))
        n = WideCharToMultiByte(CP_ACP, 0, ScriptParams[0].p, -1, ScriptParams[1].p, ScriptParams[2].i, NULL, NULL);
    script_cond(s, n > 0);
    return 0;
}

/* 0C69 ansi_string %1d% to_unicode %2d% size %3d% (characters) */
static int8_t __stdcall op_0C69(Script *s)
{
    char buf[256];
    const char *src = script_string(s, buf, sizeof buf);
    int n = 0;
    script_collect(s, 2);
    if (ScriptParams[1].i > 0 && writable(ScriptParams[0].p, (size_t)ScriptParams[1].i * 2))
        n = MultiByteToWideChar(CP_ACP, 0, src, -1, ScriptParams[0].p, ScriptParams[1].i);
    script_cond(s, n > 0);
    return 0;
}

/* 0C70 %2d% = get_module %1d% handle */
static int8_t __stdcall op_0C70(Script *s)
{
    char name[256];
    HMODULE m = GetModuleHandleA(script_string(s, name, sizeof name));
    storep(s, m);
    script_cond(s, m != NULL);
    return 0;
}

/* 0C71 %3d% = get_module %1d% proc %2d% address */
static int8_t __stdcall op_0C71(Script *s)
{
    char name[256];
    HMODULE m = (HMODULE)(uintptr_t)script_int(s);
    const char *proc = script_string(s, name, sizeof name);
    FARPROC p = m ? GetProcAddress(m, proc) : NULL;
    storep(s, (void *)p);
    script_cond(s, p != NULL);
    return 0;
}

/* 0C8D set_clipboard_text %1d% */
static int8_t __stdcall op_0C8D(Script *s)
{
    char buf[256];
    const char *text = script_string(s, buf, sizeof buf);
    size_t n = (size_t)lstrlenA(text) + 1;
    HGLOBAL h;
    if (!OpenClipboard(NULL))
        return 0;
    EmptyClipboard();
    if ((h = GlobalAlloc(GMEM_MOVEABLE, n)) != NULL) {
        memcpy(GlobalLock(h), text, n);
        GlobalUnlock(h);
        if (!SetClipboardData(CF_TEXT, h))
            GlobalFree(h);
    }
    CloseClipboard();
    return 0;
}

/* 0C8E get_clipboard_text %1d% size %2d% */
static int8_t __stdcall op_0C8E(Script *s)
{
    HANDLE h;
    const char *text;
    int ok = 0;
    script_collect(s, 2);
    if (ScriptParams[1].i > 0 && writable(ScriptParams[0].p, (size_t)ScriptParams[1].i) && OpenClipboard(NULL)) {
        if ((h = GetClipboardData(CF_TEXT)) != NULL && (text = GlobalLock(h)) != NULL) {
            copy_string(ScriptParams[0].p, text, (size_t)ScriptParams[1].i);
            GlobalUnlock(h);
            ok = 1;
        }
        CloseClipboard();
    }
    script_cond(s, ok);
    return 0;
}

/* 1337 make_script_private %1d%: nothing to hide from here */
static int8_t __stdcall op_1337(Script *s)
{
    script_collect(s, 1);
    return 0;
}

/* ---- stubs for the rest -------------------------------------------------- */

enum { SKIP_VARARGS = 1, SKIP_COND = 2 };

/* SAMPFUNCS's opcodes moonsamp has no handler for (Sanny Builder's
 * library): parameter count, then flags. Drawing, timers, downloads, global
 * functions and threads need more than a handler. */
static const struct { uint16_t op; uint8_t params, flags; } missing[] = {
    { 0x0B5F, 4, 0 }, { 0x0B60, 4, 0 }, { 0x0B68, 6, 0 }, { 0x0B69, 5, 0 },
    { 0x0B6A, 7, 0 }, { 0x0B6B, 3, 0 }, { 0x0B6C, 2, 0 }, { 0x0B6D, 4, 0 },
    { 0x0B6E, 1, SKIP_COND }, { 0x0B6F, 5, 0 }, { 0x0B70, 7, 0 }, { 0x0B71, 2, SKIP_COND },
    { 0x0B72, 1, SKIP_COND }, { 0x0B73, 7, 0 }, { 0x0B8F, 6, 0 },
    { 0x0C34, 2, SKIP_COND }, { 0x0C35, 2, SKIP_VARARGS | SKIP_COND }, { 0x0C36, 1, SKIP_VARARGS | SKIP_COND },
    { 0x0C37, 1, SKIP_COND }, { 0x0C38, 3, SKIP_COND }, { 0x0C39, 1, 0 },
    { 0x0C3B, 1, 0 }, { 0x0C3C, 0, 0 }, { 0x0C3D, 1, 0 }, { 0x0C3E, 2, 0 },
    { 0x0C3F, 2, 0 }, { 0x0C40, 1, SKIP_COND }, { 0x0C41, 2, SKIP_COND }, { 0x0C42, 2, SKIP_COND },
    { 0x0C43, 3, SKIP_COND }, { 0x0C44, 2, 0 },
    { 0x0C65, 3, 0 }, { 0x0C66, 2, SKIP_COND }, { 0x0C6A, 2, SKIP_VARARGS },
    { 0x0C72, 2, 0 }, { 0x0C73, 2, 0 }, { 0x0C74, 3, SKIP_COND }, { 0x0C75, 1, SKIP_COND },
    { 0x0C76, 1, SKIP_COND }, { 0x0C77, 2, SKIP_COND }, { 0x0C78, 2, SKIP_COND }, { 0x0C79, 1, SKIP_COND },
    { 0x0C7A, 2, SKIP_COND }, { 0x0C7B, 2, SKIP_COND }, { 0x0C7C, 2, SKIP_COND }, { 0x0C7D, 1, SKIP_COND },
    { 0x0C8C, 3, SKIP_COND },
};

static uint8_t logged[sizeof missing / sizeof missing[0]];

static int8_t __stdcall op_missing(Script *s)
{
    uint16_t op = *(uint16_t *)(s->ip - 2) & 0x7FFF;
    char tmp[256];
    size_t i;
    int k;
    for (i = 0; i < sizeof missing / sizeof missing[0] && missing[i].op != op; i++)
        ;
    if (i == sizeof missing / sizeof missing[0])
        return 0;
    if (!logged[i]) {
        logged[i] = 1;
        ms_log("opcode %04X (SAMPFUNCS) is not in moonsamp: a script used it, it does nothing", op);
    }
    for (k = 0; k < missing[i].params; k++) {
        uint8_t t = *s->ip;
        if (t >= 0x01 && t <= 0x08)
            script_collect(s, 1);
        else
            script_string(s, tmp, sizeof tmp);
    }
    if (missing[i].flags & SKIP_VARARGS)
        script_skip_varargs(s);
    if (missing[i].flags & SKIP_COND)
        script_cond(s, 0);
    return 0;
}

void sfextra_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B2D, op_0B2D }, { 0x0B2E, op_0B2E }, { 0x0B55, op_0B55 }, { 0x0B56, op_0B56 },
        { 0x0B5E, op_0B5E }, { 0x0B66, op_0B66 }, { 0x0B67, op_0B67 },
        { 0x0BFA, op_0BFA }, { 0x0BFB, op_0BFB }, { 0x0BFE, op_0BFE }, { 0x0BFF, op_0BFF },
        { 0x0C00, op_0C00 }, { 0x0C01, op_0C01 }, { 0x0C02, op_0C02 }, { 0x0C03, op_0C03 },
        { 0x0C04, op_0C04 }, { 0x0C05, op_0C05 }, { 0x0C06, op_0C06 }, { 0x0C07, op_0C07 },
        { 0x0C08, op_0C08 }, { 0x0C09, op_0C09 }, { 0x0C0A, op_0C0A }, { 0x0C0B, op_0C0B },
        { 0x0C0C, op_0C0C }, { 0x0C0D, op_0C0D }, { 0x0C0E, op_0C0E }, { 0x0C0F, op_0C0F },
        { 0x0C10, op_0C10 }, { 0x0C11, op_0C11 }, { 0x0C12, op_0C12 }, { 0x0C13, op_0C13 },
        { 0x0C14, op_0C14 }, { 0x0C15, op_0C15 }, { 0x0C16, op_0C16 }, { 0x0C17, op_0C17 },
        { 0x0C18, op_0C18 }, { 0x0C19, op_0C19 }, { 0x0C1A, op_0C1A }, { 0x0C1B, op_0C1B },
        { 0x0C1C, op_0C1C }, { 0x0C1D, op_0C1D }, { 0x0C1E, op_0C1E }, { 0x0C1F, op_0C1F },
        { 0x0C20, op_0C20 }, { 0x0C21, op_0C21 }, { 0x0C22, op_0C22 }, { 0x0C23, op_0C23 },
        { 0x0C24, op_0C24 }, { 0x0C25, op_0C25 }, { 0x0C26, op_0C26 }, { 0x0C27, op_0C27 },
        { 0x0C28, op_0C28 }, { 0x0C29, op_0C29 }, { 0x0C2A, op_0C2A }, { 0x0C2B, op_0C2B },
        { 0x0C2C, op_0C2C }, { 0x0C2D, op_0C2D }, { 0x0C2E, op_0C2E }, { 0x0C2F, op_0C2F },
        { 0x0C30, op_0C30 }, { 0x0C31, op_0C31 }, { 0x0C32, op_0C32 }, { 0x0C33, op_0C33 },
        { 0x0C3A, op_0C3A }, { 0x0C67, op_0C67 }, { 0x0C68, op_0C68 }, { 0x0C69, op_0C69 },
        { 0x0C70, op_0C70 }, { 0x0C71, op_0C71 }, { 0x0C89, op_0C89 }, { 0x0C8D, op_0C8D },
        { 0x0C8E, op_0C8E }, { 0x1337, op_1337 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
    for (i = 0; i < sizeof missing / sizeof missing[0]; i++)
        if (!script_handler(missing[i].op))
            script_try_register(missing[i].op, op_missing);
}
