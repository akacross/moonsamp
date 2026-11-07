/* Opcode host: the part of CLEO that MoonLoader relies on.
 *
 * MoonLoader runs every samp and raknet function as a script opcode: it
 * writes the parameters in SCM format, points a CRunningScript at them and
 * calls CommandHandlerTable[op / 100]. It takes the table's address from the
 * operand at 0x469FEE (CRunningScript::Process) once, at startup;
 * ProcessOneCommand has its own copy at 0x469EF0. The game's table stops at opcode
 * 0x0A8B, so without CLEO those calls land past its end.
 *
 * Without CLEO this installs a full-size table whose extra slots dispatch to
 * registered handlers. With CLEO 4 or 5 loaded, handlers go through
 * CLEO_RegisterOpcode instead and CLEO's own table is left alone. */
#include "moonsamp.h"

#define GAME_TABLE      ((void **)0x8A6168)
#define GAME_TABLE_REF  ((void *)0x469FEE)
#define GAME_TABLE_REF2 ((void *)0x469EF0)
#define GAME_SLOTS      27
#define ALL_SLOTS       (0x8000 / 100 + 1)

typedef void (THISCALL *CollectFn)(Script *, int16_t);
typedef void (THISCALL *StoreFn)(Script *, int16_t);
typedef void (THISCALL *CondFn)(Script *, uint8_t);
typedef void (THISCALL *TextLabelFn)(Script *, char *, uint8_t);
typedef int8_t (THISCALL *HandlerFn)(Script *, int);
typedef int (__stdcall *CleoRegisterFn)(uint16_t, OpcodeFn);
typedef uint32_t (__stdcall *CleoVersionFn)(void);

#define CollectParameters       ((CollectFn)0x464080)
#define StoreParameters         ((StoreFn)0x464370)
#define UpdateCompareFlag       ((CondFn)0x4859D0)
#define ReadTextLabelFromScript ((TextLabelFn)0x463D50)

static void *table[ALL_SLOTS];
static OpcodeFn handlers[0x8000];
static uint8_t warned[0x8000 / 8];
static CleoRegisterFn cleo_register;
static uint32_t cleo_version;           /* 0x0MmmPP00: 0x04040400 is 4.4.4 */

/* CLEO 5 sets this bit (bIsProcessing) while a script runs an opcode, and
 * suspends a script that starts one with it still set. */
#define CLEO5_FLAGS       0xDF
#define CLEO5_PROCESSING  0x02

/* MoonLoader reads results from local vars, strings as char* (0C46 puts its
 * text in local 8). For an opcode not implemented yet every local points at
 * zeroes: "" or 0, never a stale pointer it would read as a string. */
static uint8_t zeroes[256];

/* With moonsamp.trace in the game folder, each opcode's answer (condition
 * and local var 0) is logged whenever it changes, to see which check a
 * script stops on. */
int script_trace;
#define TRACE_SLOTS 8192
static struct { Script *s; uint16_t op; uint8_t cond; int32_t v; } traced[TRACE_SLOTS];
static int trace_lines;

/* A few probes, then the first slot is reused: MoonLoader gives every
 * lua_thread its own script, so keys keep coming and a full table must not
 * make each call scan all of it (that stalled the game for minutes). */
static void trace(Script *s, int op)
{
    unsigned first = ((uintptr_t)s * 2654435761u ^ (unsigned)op * 40503u) % TRACE_SLOTS, h = first, k;
    for (k = 0; k < 8; k++, h = (h + 1) % TRACE_SLOTS)
        if (!traced[h].s || (traced[h].s == s && traced[h].op == op))
            break;
    if (k == 8) {
        h = first;
        traced[h].s = NULL;
    }
    if (traced[h].s && traced[h].cond == s->cond_result)
        return;
    traced[h].s = s;
    traced[h].op = (uint16_t)op;
    traced[h].cond = s->cond_result;
    traced[h].v = s->locals[0];
    if (trace_lines++ < 20000)
        ms_log("trace %04X script %08lX cond %d local0 %ld", op, (DWORD)s,
               (int)s->cond_result, (long)s->locals[0]);
}

static int8_t THISCALL dispatch(Script *s, int op)
{
    int i;
    op &= 0x7FFF;
    if (handlers[op]) {
        int8_t r = handlers[op](s);
        if (script_trace)
            trace(s, op);
        return r;
    }
    if (!(warned[op >> 3] & (1 << (op & 7)))) {
        warned[op >> 3] |= 1 << (op & 7);
        ms_log("opcode %04X is not implemented", op);
    }
    for (i = 0; i < 32; i++)
        s->locals[i] = (int32_t)(uintptr_t)zeroes;
    UpdateCompareFlag(s, 0);
    return 0;
}

/* The handler is kept either way: moonsamp_run calls it directly. Returns
 * false when CLEO refuses the opcode. */
int script_try_register(uint16_t op, OpcodeFn fn)
{
    handlers[op & 0x7FFF] = fn;
    return !cleo_register || cleo_register(op, fn);
}

void script_register(uint16_t op, OpcodeFn fn)
{
    if (!script_try_register(op, fn))
        ms_log("CLEO refused opcode %04X", op);
}

OpcodeFn script_handler(uint16_t op) { return handlers[op & 0x7FFF]; }

/* For moonsamp_run only: CLEO keeps whatever handler it has. */
void script_keep(uint16_t op, OpcodeFn fn) { handlers[op & 0x7FFF] = fn; }

void script_collect(Script *s, int n) { CollectParameters(s, (int16_t)n); }
void script_store(Script *s, int n)   { StoreParameters(s, (int16_t)n); }
void script_cond(Script *s, int v)    { UpdateCompareFlag(s, v ? 1 : 0); }
int script_cleo_present(void)         { return cleo_register != NULL; }
uint32_t script_cleo_version(void)    { return cleo_version; }

int32_t script_int(Script *s)
{
    CollectParameters(s, 1);
    return ScriptParams[0].i;
}

float script_float(Script *s)
{
    CollectParameters(s, 1);
    return ScriptParams[0].f;
}

static void copy_n(char *dst, size_t size, const char *src, size_t n)
{
    if (!size)
        return;
    if (n >= size)
        n = size - 1;
    for (size_t i = 0; i < n; i++)
        dst[i] = src[i];
    dst[n] = 0;
}

/* A string parameter: an inline string (0x09 / 0x0E / 0x0F), a numeric value
 * holding a char* (MoonLoader passes strings over 127 bytes that way), or a
 * string variable. Returns buf, or the pointer itself for the char* case. */
const char *script_string(Script *s, char *buf, size_t size)
{
    uint8_t type = *s->ip;
    size_t n;
    buf[0] = 0;
    switch (type) {
    case 0x0E:
        n = s->ip[1];
        copy_n(buf, size, (const char *)s->ip + 2, n);
        s->ip += 2 + n;
        return buf;
    case 0x09:
    case 0x0F:
        n = type == 0x09 ? 8 : 16;
        copy_n(buf, size, (const char *)s->ip + 1, n);
        s->ip += 1 + n;
        return buf;
    case 0x01: case 0x02: case 0x03: case 0x04:
    case 0x05: case 0x06: case 0x07: case 0x08:
        CollectParameters(s, 1);
        return ScriptParams[0].p ? (const char *)ScriptParams[0].p : "";
    default:
        ReadTextLabelFromScript(s, buf, size > 255 ? 255 : (uint8_t)size);
        buf[size - 1] = 0;
        return buf;
    }
}

void script_skip_varargs(Script *s)
{
    char tmp[256];
    while (*s->ip) {
        uint8_t t = *s->ip;
        if (t >= 0x01 && t <= 0x08)
            CollectParameters(s, 1);
        else
            script_string(s, tmp, sizeof tmp);
    }
    s->ip++;
}

/* Runs a callback stub: MoonLoader hands SAMPFUNCS a label (a negative
 * offset from the script's base) whose code calls back into MoonLoader with
 * 0AA5 and ends with a return opcode (0B43 for commands, 0BE0 for RakNet
 * hooks). SAMPFUNCS runs it on the spot; so does this. */
static int label_done;

void script_label_return(void) { label_done = 1; }

/* ProcessOneCommand for a callback stub. Its opcodes (0AA5 and a return)
 * run on moonsamp's own handlers, never through CLEO: CLEO 5 marks a script
 * inside an opcode, and MoonLoader runs opcodes on this same script while
 * 0AA5 calls it, which CLEO 5 then refuses (silently, for a script it did
 * not start). Anything else goes through the table at 0x469FEE: CLEO 4
 * replaces only that one, so the copy ProcessOneCommand reads (0x469EF0) is
 * still the game's 27-slot table. */
static void step(Script *s)
{
    uint16_t op = *(uint16_t *)s->ip;
    (*(uint16_t *)0xA447F4)++;              /* CTheScripts::CommandsExecuted */
    s->ip += 2;
    ((uint8_t *)s)[0xD2] = op >> 15;        /* NotFlag */
    op &= 0x7FFF;
    if (handlers[op])
        handlers[op](s);
    else
        (*(HandlerFn **)GAME_TABLE_REF)[op / 100](s, op);
}

int script_run_label(Script *s, int32_t label)
{
    uint8_t *saved_ip = s->ip, flags = ((uint8_t *)s)[CLEO5_FLAGS];
    int saved_done = label_done, cleo5 = cleo_version >= 0x05000000;
    ScriptParam saved_params[8];
    int i, ok;
    for (i = 0; i < 8; i++)
        saved_params[i] = ScriptParams[i];
    /* A hook can fire while this script is inside an opcode: under CLEO 5
     * the opcodes MoonLoader runs on it from the stub must not look like the
     * next one in its place. */
    if (cleo5)
        ((uint8_t *)s)[CLEO5_FLAGS] &= (uint8_t)~CLEO5_PROCESSING;
    s->ip = s->base_ip - label;
    label_done = 0;
    for (i = 0; i < 256 && !label_done; i++)
        step(s);
    ok = label_done;
    if (!ok)
        ms_log("callback stub of %.8s did not return", s->name);
    label_done = saved_done;
    s->ip = saved_ip;
    if (cleo5)
        ((uint8_t *)s)[CLEO5_FLAGS] = (uint8_t)((((uint8_t *)s)[CLEO5_FLAGS] & ~CLEO5_PROCESSING) |
                                               (flags & CLEO5_PROCESSING));
    for (i = 0; i < 8; i++)
        ScriptParams[i] = saved_params[i];
    return ok;
}

/* 0AA5 call_function addr num_params pop args... (CLEO). Arguments are
 * listed last-first, so they are pushed in the order listed. */
static int8_t __stdcall op_0AA5(Script *s)
{
    uint32_t args[16], *argp = args;
    int n, i;
    void *fn;
    script_collect(s, 3);
    fn = ScriptParams[0].p;
    n = ScriptParams[1].i;
    if (n < 0 || n > 16)
        n = 0;
    for (i = 0; i < n; i++)
        args[i] = (uint32_t)script_int(s);
    script_skip_varargs(s);
    __asm__ volatile(
        "movl %%esp, %%esi\n\t"
        "xorl %%ebx, %%ebx\n"
        "1:\n\t"
        "cmpl %%ecx, %%ebx\n\t"
        "jge 2f\n\t"
        "pushl (%%edx,%%ebx,4)\n\t"
        "incl %%ebx\n\t"
        "jmp 1b\n"
        "2:\n\t"
        "call *%%eax\n\t"
        "movl %%esi, %%esp\n\t"
        : "+a"(fn), "+c"(n), "+d"(argp)
        :
        : "ebx", "esi", "memory", "cc");
    return 0;
}

/* CLEO.asi, loaded now if it is installed but not loaded yet: in the game
 * folder, or in scripts\ or plugins\ where ASI loaders look too (CLEO 5.5
 * may sit there), so its opcode table is in place before moonsamp's
 * handlers go to it. */
static HMODULE load_cleo(void)
{
    static const char *const dirs[] = { "", "scripts\\", "plugins\\" };
    char path[MAX_PATH];
    HMODULE cleo = GetModuleHandleA("CLEO.asi");
    int k, n;
    if (cleo)
        return cleo;
    GetModuleFileNameA(NULL, path, sizeof path);
    for (n = lstrlenA(path); n > 0 && path[n - 1] != '\\' && path[n - 1] != '/'; n--)
        ;
    for (k = 0; k < 3; k++) {
        if (n + lstrlenA(dirs[k]) + 9 > (int)sizeof path)
            continue;
        lstrcpyA(path + n, dirs[k]);
        lstrcatA(path + n, "CLEO.asi");
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
            continue;
        if ((cleo = LoadLibraryA(path)) != NULL)
            return cleo;
    }
    return NULL;
}

static FARPROC cleo_proc(HMODULE cleo, const char *decorated, const char *plain)
{
    FARPROC p = GetProcAddress(cleo, decorated);
    return p ? p : GetProcAddress(cleo, plain);
}

int script_host_install(void)
{
    HMODULE cleo = load_cleo();
    int i;

    script_keep(0x0AA5, op_0AA5);
    if (cleo) {
        CleoVersionFn version = (CleoVersionFn)(void *)cleo_proc(cleo, "_CLEO_GetVersion@0", "CLEO_GetVersion");
        cleo_register = (CleoRegisterFn)(void *)cleo_proc(cleo, "_CLEO_RegisterOpcode@8", "CLEO_RegisterOpcode");
        cleo_version = version ? version() : 0;
        if (cleo_register) {
            ms_log("CLEO %d.%d.%d found: registering opcodes through it", (int)(cleo_version >> 24),
                   (int)(cleo_version >> 16 & 0xFF), (int)(cleo_version >> 8 & 0xFF));
            return 1;
        }
        ms_log("CLEO.asi has no CLEO_RegisterOpcode; not starting");
        return 0;
    }

    if (*(void **)GAME_TABLE_REF != GAME_TABLE || *(void **)GAME_TABLE_REF2 != GAME_TABLE) {
        ms_log("opcode table already moved (%08lX, %08lX); not starting",
               *(DWORD *)GAME_TABLE_REF, *(DWORD *)GAME_TABLE_REF2);
        return 0;
    }
    for (i = 0; i < GAME_SLOTS; i++)
        table[i] = GAME_TABLE[i];
    for (; i < ALL_SLOTS; i++)
        table[i] = (void *)dispatch;
    {
        void *p = table;
        if (!ms_patch(GAME_TABLE_REF, &p, 4) || !ms_patch(GAME_TABLE_REF2, &p, 4))
            return 0;
    }
    ms_log("opcode table installed (%d slots)", ALL_SLOTS);
    return 1;
}
