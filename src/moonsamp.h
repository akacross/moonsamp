#ifndef MOONSAMP_H
#define MOONSAMP_H

#include <stdint.h>
#include <windows.h>

#define THISCALL __attribute__((thiscall))

/* CRunningScript, GTA:SA 1.0 US (gta-reversed RunningScript.h). */
typedef struct Script {
    struct Script *next, *prev;
    char name[8];
    uint8_t *base_ip;
    uint8_t *ip;
    uint8_t *ip_stack[8];
    uint16_t stack_depth;
    uint16_t pad_3a;
    int32_t locals[34];
    uint8_t is_active;
    uint8_t cond_result;
} Script;

typedef union {
    int32_t i;
    uint32_t u;
    float f;
    void *p;
} ScriptParam;

#define ScriptParams ((ScriptParam *)0xA43C78)

typedef int8_t (__stdcall *OpcodeFn)(Script *s);

/* main.c */
void ms_log(const char *fmt, ...);
int ms_patch(void *addr, const void *bytes, size_t n);

/* script.c */
int script_host_install(void);
int script_cleo_present(void);
extern int script_trace;
void script_register(uint16_t op, OpcodeFn fn);
void script_collect(Script *s, int n);
void script_store(Script *s, int n);
void script_cond(Script *s, int value);
int32_t script_int(Script *s);
float script_float(Script *s);
const char *script_string(Script *s, char *buf, size_t size);
void script_skip_varargs(Script *s);
int script_run_label(Script *s, int32_t label);
void script_label_return(void);

#define AT(p, off, T) (*(T *)((uint8_t *)(p) + (off)))

/* One samp.dll build: RVAs of what moonsamp calls, and struct offsets. */
typedef struct {
    const char *name;
    uint32_t entry;                 /* AddressOfEntryPoint: identifies the build */
    uint32_t netgame, chat, input;  /* globals holding the object pointers */
    uint32_t add_message;           /* CChat::AddMessage(color, text) */
    uint32_t local_chat;            /* CLocalPlayer::Chat(text) */
    uint32_t send_command;          /* sends "/cmd args" to the server */
    uint32_t player_color;          /* stdcall RGBA GetPlayerColor(id) */
    uint16_t pp_largest, pp_used, pp_info;
    uint16_t pp_local_id, pp_local_name, pp_local_obj;
    uint8_t info_remote, info_name, remote_ped;
    uint32_t handle_rpc_call;       /* the call to HandleRPCPacket in RakClient::Receive */
    uint32_t handle_rpc;            /* RakPeer::HandleRPCPacket(data, length, PlayerID) */
    uint32_t string_compressor;     /* global StringCompressor* */
    uint32_t encode_string, decode_string;
    uint32_t bs_ctor, bs_dtor;      /* RakNet::BitStream() / ~BitStream() */
    uint32_t dialog, game, scoreboard;              /* globals */
    uint32_t dialog_show, dialog_close;             /* CDialog::Show / Close */
    uint32_t listbox_selected, listbox_select;      /* CDXUTListBox::GetSelectedIndex / SelectItem */
    uint32_t editbox_get, editbox_set;              /* CDXUTEditBox::GetText / SetText */
    uint32_t shutdown_restart;                      /* CNetGame::ShutdownForRestart */
    uint32_t set_cursor_mode;                       /* CGame::SetCursorMode */
    uint32_t set_local_name;                        /* CPlayerPool::SetLocalPlayerName */
    uint32_t vehicle_find;                          /* CVehiclePool::Find(CVehicle *) */
    uint32_t special_action;                        /* CLocalPlayer::GetSpecialAction */
    uint32_t textdraw_set_text;                     /* CTextDraw::SetText */
    uint16_t info_ping, pp_local_ping;
} Build;

/* samp.c */
extern uintptr_t samp_base;
const Build *samp_build(void);
void *samp_netgame(void);
void *samp_global(uint32_t rva);
void *samp_pool(int slot);
void *samp_local_player(void);
int samp_install(void);
void samp_tick(void);
void copy_string(char *dst, const char *src, size_t size);
void samp_run_command(Script *s, int32_t label, const char *params);

/* Pools: CNetGame + 0x3DE -> SPools, the same in every supported build. */
#define POOL_PLAYER   0x08
#define POOL_VEHICLE  0x0C
#define POOL_GANGZONE 0x18
#define POOL_LABEL    0x1C
#define POOL_TEXTDRAW 0x20

/* dialogs.c */
void dialogs_install(void);
int dialog_response_sent(const uint8_t *data, int bits);

/* textdraws.c */
void textdraws_install(void);

/* console.c */
void console_install(void);

/* raknet.c */
void raknet_install(void);
void raknet_tick(void);

#endif
