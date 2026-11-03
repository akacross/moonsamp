/* SAMPFUNCS's console and global variables.
 *
 * There is no console window: isSampfuncsConsoleActive() is false and
 * sampfuncsLog writes to moonsamp.log. Console commands are kept and run by
 * runSampfuncsConsoleCommand, through the same callback stub as chat
 * commands. Global variables are shared by every script, all of which may
 * read and write them (the mods share flags and epochs this way). */
#include "moonsamp.h"

#define MAX_CONSOLE_COMMANDS 64
#define MAX_VARS             256

static struct {
    char name[33];
    Script *script;
    int32_t label;
} commands[MAX_CONSOLE_COMMANDS];

static struct {
    char name[64];
    int32_t value;
} vars[MAX_VARS];

static int find_command(const char *name)
{
    int i;
    for (i = 0; i < MAX_CONSOLE_COMMANDS; i++)
        if (commands[i].script && lstrcmpiA(commands[i].name, name) == 0)
            return i;
    return -1;
}

static int find_var(const char *name)
{
    int i;
    for (i = 0; i < MAX_VARS; i++)
        if (vars[i].name[0] && lstrcmpA(vars[i].name, name) == 0)
            return i;
    return -1;
}

static const char *arg_string(Script *s, char *buf, size_t size)
{
    copy_string(buf, script_string(s, buf, size), size);
    return buf;
}

/* 0B78 log %1s% ...: colour tags like {FF0000} left out. MoonLoader mirrors
 * its whole log here; those lines ("[ML] ...") are already in moonloader.log. */
static int8_t __stdcall op_0B78(Script *s)
{
    char text[512], out[512];
    int i, j = 0;
    arg_string(s, text, sizeof text);
    script_skip_varargs(s);
    for (i = 0; text[i] == '{' && lstrlenA(text + i) >= 8 && text[i + 7] == '}'; i += 8)
        ;
    if (text[i] == '[' && text[i + 1] == 'M' && text[i + 2] == 'L' && text[i + 3] == ']')
        return 0;
    for (i = 0; text[i] && j < (int)sizeof out - 1; i++) {
        if (text[i] == '{' && lstrlenA(text + i) >= 8 && text[i + 7] == '}') {
            i += 7;
            continue;
        }
        out[j++] = text[i];
    }
    out[j] = 0;
    ms_log("console: %s", out);
    return 0;
}

/* 0C7E is_console_active */
static int8_t __stdcall op_0C7E(Script *s)
{
    script_cond(s, 0);
    return 0;
}

/* 0C63 register_console_command %1d% to_label %2p% */
static int8_t __stdcall op_0C63(Script *s)
{
    char name[64];
    int32_t label;
    int i;
    arg_string(s, name, sizeof name);
    label = script_int(s);
    if (!name[0] || lstrlenA(name) > 32 || find_command(name) >= 0) {
        script_cond(s, 0);
        return 0;
    }
    for (i = 0; i < MAX_CONSOLE_COMMANDS && commands[i].script; i++)
        ;
    if (i < MAX_CONSOLE_COMMANDS) {
        copy_string(commands[i].name, name, sizeof commands[i].name);
        commands[i].label = label;
        commands[i].script = s;
    }
    script_cond(s, i < MAX_CONSOLE_COMMANDS);
    return 0;
}

/* 0C64 unregister_console_command %1d% */
static int8_t __stdcall op_0C64(Script *s)
{
    char name[64];
    int i = find_command(arg_string(s, name, sizeof name));
    if (i >= 0)
        commands[i].script = NULL;
    script_cond(s, i >= 0);
    return 0;
}

/* 0C91 is_console_command_defined %1d% */
static int8_t __stdcall op_0C91(Script *s)
{
    char name[64];
    script_cond(s, find_command(arg_string(s, name, sizeof name)) >= 0);
    return 0;
}

/* 0C62 exec_console_command %1d%: "name params" */
static int8_t __stdcall op_0C62(Script *s)
{
    char line[512], name[33];
    const char *params;
    int n, i;
    arg_string(s, line, sizeof line);
    for (params = line; *params == ' '; params++)
        ;
    for (n = 0; params[n] && params[n] != ' ' && n < 32; n++)
        name[n] = params[n];
    name[n] = 0;
    params += n;
    while (*params == ' ')
        params++;
    if ((i = find_command(name)) >= 0)
        samp_run_command(commands[i].script, commands[i].label, params);
    else if (name[0])
        ms_log("console: unknown command \"%s\"", name);
    return 0;
}

/* 0C7F samp set_client_command %1d% description %2d%, 0C80 set_console_command
 * ...: descriptions are only shown in SAMPFUNCS's console. */
static int8_t __stdcall op_describe(Script *s)
{
    char a[64], b[256];
    arg_string(s, a, sizeof a);
    arg_string(s, b, sizeof b);
    return 0;
}

/* 0BFC set_global_var %1d% = %2d% */
static int8_t __stdcall op_0BFC(Script *s)
{
    char name[64];
    int i;
    int32_t v;
    arg_string(s, name, sizeof name);
    v = script_int(s);
    i = find_var(name);
    if (i < 0 && name[0])
        for (i = 0; i < MAX_VARS && vars[i].name[0]; i++)
            ;
    if (i >= 0 && i < MAX_VARS) {
        copy_string(vars[i].name, name, sizeof vars[i].name);
        vars[i].value = v;
    }
    script_cond(s, i >= 0 && i < MAX_VARS);
    return 0;
}

/* 0BFD %2d% = get_global_var %1d% */
static int8_t __stdcall op_0BFD(Script *s)
{
    char name[64];
    int i = find_var(arg_string(s, name, sizeof name));
    ScriptParams[0].i = i >= 0 ? vars[i].value : 0;
    script_store(s, 1);
    script_cond(s, i >= 0);
    return 0;
}

/* 0C5F is_global_var %1d% defined */
static int8_t __stdcall op_0C5F(Script *s)
{
    char name[64];
    script_cond(s, find_var(arg_string(s, name, sizeof name)) >= 0);
    return 0;
}

/* 0C61 get_global_var %1d% access_for_thread %2d% read_to %3d% write_to %4d% */
static int8_t __stdcall op_0C61(Script *s)
{
    char name[64];
    int defined = find_var(arg_string(s, name, sizeof name)) >= 0;
    script_int(s);
    ScriptParams[0].i = defined;
    ScriptParams[1].i = defined;
    script_store(s, 2);
    return 0;
}

/* 0C60 set_global_var %1d% access_for_thread %2d% read %3d% write %4d%:
 * everyone may already read and write. */
static int8_t __stdcall op_0C60(Script *s)
{
    char name[64];
    arg_string(s, name, sizeof name);
    script_collect(s, 3);
    script_cond(s, find_var(name) >= 0);
    return 0;
}

void console_install(void)
{
    static const struct { uint16_t op; OpcodeFn fn; } ops[] = {
        { 0x0B78, op_0B78 }, { 0x0C7E, op_0C7E }, { 0x0C63, op_0C63 }, { 0x0C64, op_0C64 },
        { 0x0C91, op_0C91 }, { 0x0C62, op_0C62 }, { 0x0C7F, op_describe }, { 0x0C80, op_describe },
        { 0x0BFC, op_0BFC }, { 0x0BFD, op_0BFD }, { 0x0C5F, op_0C5F }, { 0x0C61, op_0C61 },
        { 0x0C60, op_0C60 },
    };
    size_t i;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        script_register(ops[i].op, ops[i].fn);
}
