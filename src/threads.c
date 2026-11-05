/* GTA script threads: pause and resume any, and threads running SCM code
 * at a pointer, with arguments in their locals. GTA walks its active list
 * while scripts run, so a thread leaves it only in the frame tick. */
#include "moonsamp.h"

#define IDLE_SCRIPTS   (*(Script **)0xA8B428)     /* CTheScripts::pIdleScripts */
#define ACTIVE_SCRIPTS (*(Script **)0xA8B42C)     /* CTheScripts::pActiveScripts */
#define WAKE_TIME(t)   AT(t, 0xCC, uint32_t)
#define NEVER          0xFFFFFFFFu
#define MAX_THREADS    64

typedef Script *(__cdecl *StartNewScriptFn)(uint8_t *ip);
#define StartNewScript ((StartNewScriptFn)0x464C20)

static struct { Script *script; uint32_t wake; } paused[MAX_THREADS];
static struct { Script *script; int dying; } custom[MAX_THREADS];

static int custom_slot(Script *t)
{
    int i;
    for (i = 0; i < MAX_THREADS; i++)
        if (t && custom[i].script == t)
            return i;
    return -1;
}

/* The argument list after an opcode's fixed parameters, into t's locals:
 * numbers as they are, a string as 0. */
static void read_args(Script *s, Script *t)
{
    int n = 0;
    while (*s->ip) {
        if (*s->ip == 0x0E) {
            s->ip += 2 + s->ip[1];
            ScriptParams[0].i = 0;
        } else {
            script_collect(s, 1);
        }
        if (t && n < 32)
            t->locals[n] = ScriptParams[0].i;
        n++;
    }
    s->ip++;
}

static void start_at(Script *t, uint8_t *ip)
{
    t->base_ip = t->ip = ip;
    t->stack_depth = 0;
    t->cond_result = 0;
    WAKE_TIME(t) = 0;
}

/* 0C6B %2d% = create_custom_thread_at_pointer %1d% args... */
static int8_t __stdcall op_0C6B(Script *s)
{
    uint8_t *ip = (uint8_t *)(uintptr_t)script_int(s);
    Script *t = NULL;
    int i;
    for (i = 0; i < MAX_THREADS && custom[i].script; i++)
        ;
    if (ip && IDLE_SCRIPTS && i < MAX_THREADS) {
        t = StartNewScript(ip);
        start_at(t, ip);
        lstrcpynA(t->name, "custom", sizeof t->name);
        custom[i].script = t;
        custom[i].dying = 0;
    }
    ScriptParams[0].p = t;
    script_store(s, 1);
    read_args(s, t);
    return 0;
}

/* 0C6F restart_custom_thread %1d% args... */
static int8_t __stdcall op_0C6F(Script *s)
{
    Script *t = (Script *)(uintptr_t)script_int(s);
    int i = custom_slot(t);
    if (i < 0 || custom[i].dying)
        t = NULL;
    if (t)
        start_at(t, t->base_ip);
    read_args(s, t);
    return 0;
}

/* 0C6E destroy_custom_thread %1d%: stops now, leaves GTA's list in the tick */
static int8_t __stdcall op_0C6E(Script *s)
{
    Script *t = (Script *)(uintptr_t)script_int(s);
    int i = custom_slot(t);
    if (i >= 0) {
        WAKE_TIME(t) = NEVER;
        custom[i].dying = 1;
    }
    return 0;
}

/* 0C6C set_thread %1d% local_var %2d% = %3d% */
static int8_t __stdcall op_0C6C(Script *s)
{
    Script *t;
    script_collect(s, 3);
    t = ScriptParams[0].p;
    if (t && (unsigned)ScriptParams[1].i < 32)
        t->locals[ScriptParams[1].i] = ScriptParams[2].i;
    return 0;
}

/* 0C6D %3d% = get_thread %1d% local_var %2d% */
static int8_t __stdcall op_0C6D(Script *s)
{
    Script *t;
    int var;
    script_collect(s, 2);
    t = ScriptParams[0].p;
    var = ScriptParams[1].i;
    ScriptParams[0].i = t && (unsigned)var < 32 ? t->locals[var] : 0;
    script_store(s, 1);
    return 0;
}

/* 0BDE pause_thread %1d%: it never wakes until resumed */
static int8_t __stdcall op_0BDE(Script *s)
{
    Script *t = (Script *)(uintptr_t)script_int(s);
    int i, free_slot = -1;
    if (!t || t == s)
        return 0;
    for (i = 0; i < MAX_THREADS; i++) {
        if (paused[i].script == t)
            return 0;
        if (!paused[i].script && free_slot < 0)
            free_slot = i;
    }
    if (free_slot >= 0) {
        paused[free_slot].script = t;
        paused[free_slot].wake = WAKE_TIME(t);
        WAKE_TIME(t) = NEVER;
    }
    return 0;
}

/* 0BDF resume_thread %1d%: it wakes when it would have */
static int8_t __stdcall op_0BDF(Script *s)
{
    Script *t = (Script *)(uintptr_t)script_int(s);
    int i;
    for (i = 0; t && i < MAX_THREADS; i++)
        if (paused[i].script == t) {
            WAKE_TIME(t) = paused[i].wake;
            paused[i].script = NULL;
        }
    return 0;
}

/* Once a frame, outside GTA's script loop: destroyed threads go back to the
 * idle list. */
void threads_tick(void)
{
    int i, k;
    for (i = 0; i < MAX_THREADS; i++) {
        Script *t = custom[i].script;
        if (!t || !custom[i].dying)
            continue;
        if (t->prev)
            t->prev->next = t->next;
        else
            ACTIVE_SCRIPTS = t->next;
        if (t->next)
            t->next->prev = t->prev;
        t->is_active = 0;
        t->prev = NULL;
        t->next = IDLE_SCRIPTS;
        if (IDLE_SCRIPTS)
            IDLE_SCRIPTS->prev = t;
        IDLE_SCRIPTS = t;
        for (k = 0; k < MAX_THREADS; k++)
            if (paused[k].script == t)
                paused[k].script = NULL;
        custom[i].script = NULL;
    }
}

void threads_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0C6B, op_0C6B }, { 0x0C6F, op_0C6F }, { 0x0C6E, op_0C6E }, { 0x0C6C, op_0C6C },
        { 0x0C6D, op_0C6D }, { 0x0BDE, op_0BDE }, { 0x0BDF, op_0BDF },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
