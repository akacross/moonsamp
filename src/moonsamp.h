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
int script_try_register(uint16_t op, OpcodeFn fn);
OpcodeFn script_handler(uint16_t op);
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

/* Indexes into Build.pool: which SPools slot holds each pool. */
enum { POOL_PLAYER, POOL_VEHICLE, POOL_GANGZONE, POOL_LABEL, POOL_TEXTDRAW,
       POOL_OBJECT, POOL_PICKUP, POOL_ACTOR, POOL_MENU, POOL_COUNT };

/* Indexes into Build.state: CNetGame's game state values. */
enum { GS_WAITING, GS_CONNECTING, GS_CONNECTED, GS_AWAITING_JOIN, GS_RESTARTING, GS_COUNT };

/* One samp.dll build: RVAs of what moonsamp calls, and struct offsets
 * (src/builds.h). */
typedef struct {
    const char *name;
    uint32_t entry;                 /* AddressOfEntryPoint: identifies the build */
    /* globals holding the object pointers */
    uint32_t netgame, chat, input, dialog, game, scoreboard, string_compressor, death_window;
    uint32_t sendrates;             /* on-foot, in-car and aim send rates (ints) */
    uint32_t device;                /* the real IDirect3DDevice9 behind SA-MP's proxy */
    /* functions */
    uint32_t add_message;           /* CChat::AddMessage(color, text) */
    uint32_t local_chat;            /* CLocalPlayer::Chat(text) */
    uint32_t send_command;          /* sends "/cmd args" to the server */
    uint32_t player_color;          /* stdcall RGBA GetPlayerColor(id) */
    uint32_t handle_rpc_call;       /* the call to HandleRPCPacket in RakClient::Receive */
    uint32_t handle_rpc;            /* RakPeer::HandleRPCPacket(data, length, PlayerID) */
    uint32_t encode_string, decode_string;          /* StringCompressor */
    uint32_t bs_ctor, bs_dtor;                      /* RakNet::BitStream() / ~BitStream() */
    uint32_t dialog_show, dialog_close;             /* CDialog::Show / Close */
    uint32_t listbox_selected, listbox_select;      /* CDXUTListBox::GetSelectedIndex / SelectItem */
    uint32_t editbox_get, editbox_set;              /* CDXUTEditBox::GetText / SetText */
    uint32_t shutdown_restart;                      /* CNetGame::ShutdownForRestart */
    uint32_t set_cursor_mode;                       /* CGame::SetCursorMode */
    uint32_t set_local_name;                        /* CPlayerPool::SetLocalPlayerName */
    uint32_t vehicle_find;                          /* CVehiclePool::Find(CVehicle *) */
    uint32_t special_action;                        /* CLocalPlayer::GetSpecialAction */
    uint32_t textdraw_set_text;                     /* CTextDraw::SetText */
    uint32_t update_players;                        /* CNetGame::UpdatePlayers (scores, pings) */
    uint32_t current_anim;                          /* CPed::GetCurrentAnimation: id | flags << 16 */
    uint32_t anim_entry;                            /* "FILE:NAME" of SA-MP animation id, or NULL */
    uint32_t process_input;                         /* CGame::ProcessInputEnabling */
    uint32_t local_spawn, local_set_special;         /* CLocalPlayer::Spawn / SetSpecialAction */
    uint32_t input_open, input_close;               /* CInput::Open / Close */
    uint32_t scoreboard_enable, scoreboard_close;   /* CScoreboard::Enable / Close */
    uint32_t td_create, td_delete;                  /* CTextDrawPool::Create / Delete */
    uint32_t label_create, label_delete;            /* CLabelPool::Create / Delete */
    /* CLocalPlayer's senders */
    uint32_t send_unoccupied, send_onfoot, send_aim, send_trailer, send_passenger;
    uint32_t send_stats, update_weapons, send_incar;
    /* CNetGame: hostname at host + 0x101, port at host + 0x205, last
     * connect attempt at state + 4 */
    uint16_t ng_rakclient, ng_host, ng_state, ng_pools;
    uint8_t pool[POOL_COUNT];
    uint8_t state[GS_COUNT];
    /* CPlayerPool */
    uint16_t pp_largest, pp_used, pp_info;
    uint16_t pp_local_id, pp_local_name, pp_local_obj, pp_local_ping, pp_local_score;
    /* CPlayerInfo */
    uint8_t info_remote, info_name, info_ping, info_score, info_npc;
    /* CRemotePlayer: health at armour + 4; the last sync data, the animation
     * at on-foot + 0x40 and the trailer's right after the in-car data */
    uint16_t remote_ped, remote_special, remote_armour, remote_status;
    uint16_t remote_onfoot, remote_incar, remote_passenger, remote_aim;
    /* CLocalPlayer, CGame */
    uint16_t local_ped, local_active, game_cursor;
    /* CObjectPool: the CObject* array; used flags from +4, so it holds
     * (obj_objects - 4) / 4 objects */
    uint16_t obj_objects;
} Build;

/* newopcodes.c */
void newopcodes_install(void);

/* samp.c */
extern uintptr_t samp_base;
const Build *samp_build(void);
void *samp_netgame(void);
void *samp_global(uint32_t rva);
void *samp_pool(int which);
void *samp_rakclient(void);
void *samp_rakpeer(void);
int samp_cursor_mode(void);
void samp_set_cursor_mode(int mode);
int samp_typing(void);
int samp_state(void);
void *samp_local_player(void);
void *samp_remote_player(int id);
int samp_vehicle_id(int handle);
int samp_install(void);
void samp_tick(void);
void copy_string(char *dst, const char *src, size_t size);
#define ML_BUFFER 4096 /* MoonLoader's text_to buffers */
void samp_run_command(Script *s, int32_t label, const char *params);
uint32_t abgr_argb(uint32_t c);


/* dialogs.c */
void dialogs_install(void);
int dialog_response_sent(const uint8_t *data, int bits);

/* textdraws.c */
void textdraws_install(void);

/* threads.c */
void threads_install(void);
void threads_tick(void);

/* sends.c */
void sends_install(void);

/* info.c */
void info_install(void);

/* overlay.c */
enum { FONT_UI, FONT_CONSOLE, FONT_COUNT };
#define TEXT_RAW 0x100              /* or'd into a font: colour codes are text */
enum { EDIT_HANDLED = 1, EDIT_CHANGED = 2, EDIT_ENTER = 4 };
typedef struct { char text[256]; int len, caret, scroll; } EditLine;
void overlay_tick(void);
int overlay_width(void);
int overlay_height(void);
int overlay_line_height(int font);
int overlay_text_width(int font, const char *s, int n);
uint32_t overlay_color_at(const char *s, int n, uint32_t argb);
int overlay_text(int font, int x, int y, const char *s, uint32_t argb, int max_w);
void overlay_rect(int x, int y, int w, int h, uint32_t argb);
void overlay_frame(int x, int y, int w, int h, uint32_t argb);
void overlay_cursor(int *x, int *y);
void overlay_point(UINT msg, LPARAM lp, int *x, int *y);
HWND overlay_window(void);
int edit_key(EditLine *e, UINT msg, WPARAM wp);
void edit_set(EditLine *e, const char *text);
void edit_draw(EditLine *e, int font, int x, int y, int w, uint32_t argb, int focused);

/* dxut.c */
void dxut_install(void);
int dxut_visible(void);
void dxut_render(void);
int dxut_input(UINT msg, WPARAM wp, LPARAM lp);

/* console.c */
void console_install(void);
int console_visible(void);
void console_render(void);
int console_input(UINT msg, WPARAM wp, LPARAM lp);
void console_print(const char *line);
void console_tick(void);

/* raknet.c */
void raknet_install(void);
void raknet_tick(void);
int raknet_send(int rpc, const uint8_t *data, int bits, int reliability);

#endif
