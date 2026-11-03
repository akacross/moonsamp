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
 * registered handlers. With CLEO loaded, handlers go through
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
typedef int8_t (THISCALL *ProcessOneFn)(Script *);
typedef int (__stdcall *CleoRegisterFn)(uint16_t, OpcodeFn);

#define CollectParameters       ((CollectFn)0x464080)
#define StoreParameters         ((StoreFn)0x464370)
#define UpdateCompareFlag       ((CondFn)0x4859D0)
#define ReadTextLabelFromScript ((TextLabelFn)0x463D50)
#define ProcessOneCommand       ((ProcessOneFn)0x469EB0)

static void *table[ALL_SLOTS];
static OpcodeFn handlers[0x8000];
static uint8_t warned[0x8000 / 8];
static CleoRegisterFn cleo_register;

/* MoonLoader reads results from local vars 0-7, strings as char*. For an
 * opcode not implemented yet they point at zeroes: "" or 0, never garbage. */
static uint8_t zeroes[256];

static int8_t THISCALL dispatch(Script *s, int op)
{
    int i;
    op &= 0x7FFF;
    if (handlers[op])
        return handlers[op](s);
    if (!(warned[op >> 3] & (1 << (op & 7)))) {
        warned[op >> 3] |= 1 << (op & 7);
        ms_log("opcode %04X is not implemented", op);
    }
    for (i = 0; i < 8; i++)
        s->locals[i] = (int32_t)(uintptr_t)zeroes;
    UpdateCompareFlag(s, 0);
    return 0;
}

void script_register(uint16_t op, OpcodeFn fn)
{
    if (cleo_register) {
        if (!cleo_register(op, fn))
            ms_log("CLEO refused opcode %04X", op);
        return;
    }
    handlers[op & 0x7FFF] = fn;
}

void script_collect(Script *s, int n) { CollectParameters(s, (int16_t)n); }
void script_store(Script *s, int n)   { StoreParameters(s, (int16_t)n); }
void script_cond(Script *s, int v)    { UpdateCompareFlag(s, v ? 1 : 0); }
int script_cleo_present(void)         { return cleo_register != NULL; }

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

int script_run_label(Script *s, int32_t label)
{
    uint8_t *saved_ip = s->ip;
    int saved_done = label_done;
    ScriptParam saved_params[8];
    int i, ok;
    for (i = 0; i < 8; i++)
        saved_params[i] = ScriptParams[i];
    s->ip = s->base_ip - label;
    label_done = 0;
    for (i = 0; i < 256 && !label_done; i++)
        ProcessOneCommand(s);
    ok = label_done;
    if (!ok)
        ms_log("callback stub of %.8s did not return", s->name);
    label_done = saved_done;
    s->ip = saved_ip;
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

int script_host_install(void)
{
    char path[MAX_PATH];
    HMODULE cleo;
    DWORD attr;
    int i;

    GetModuleFileNameA(NULL, path, sizeof path);
    for (i = lstrlenA(path); i > 0 && path[i - 1] != '\\' && path[i - 1] != '/'; i--)
        ;
    lstrcpyA(path + i, "CLEO.asi");
    attr = GetFileAttributesA(path);
    cleo = GetModuleHandleA("CLEO.asi");
    if (!cleo && attr != INVALID_FILE_ATTRIBUTES)
        cleo = LoadLibraryA(path);
    if (cleo) {
        FARPROC p = GetProcAddress(cleo, "_CLEO_RegisterOpcode@8");
        if (!p)
            p = GetProcAddress(cleo, "CLEO_RegisterOpcode");
        cleo_register = (CleoRegisterFn)(void *)p;
        if (cleo_register) {
            ms_log("CLEO found: registering opcodes through it");
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
    script_register(0x0AA5, op_0AA5);
    ms_log("opcode table installed (%d slots)", ALL_SLOTS);
    return 1;
}
