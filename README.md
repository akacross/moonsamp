# moonsamp

The SA-MP API for MoonLoader without SAMPFUNCS or CLEO. Drop `moonsamp.asi`
into the game folder; scripts run unchanged.

It stays off when `SAMPFUNCS.asi` is installed, and registers its opcodes
through CLEO when `CLEO.asi` is installed: CLEO 4 or CLEO 5, in the game
folder, `scripts\` or `plugins\`. SAMPFUNCS does not run beside CLEO 5;
with moonsamp, MoonLoader scripts and CLEO 5 scripts run together.

What it has:

- **Every SA-MP function MoonLoader has** (`samp*`, `raknet*`, `sampfuncs*`,
  the dxut dialogs): every opcode MoonLoader emits, on 0.3.7-R1 to R5 and
  0.3.DL. See [SA-MP functions](#sa-mp-functions).
- **A console** like SAMPFUNCS's, on `~`, showing moonloader.log and
  sampfuncsLog. See [Console and dialogs](#console-and-dialogs).
- **CLEO 5's own opcodes** for MoonLoader scripts, with or without CLEO:
  keys, debug output, files, memory, audio streams, text, bits, INI.
  `moonloader/lib/cleo5.lua`, every function in [docs/cleo5.md](docs/cleo5.md).
- **All of NewOpcodes** (DK22Pac's plugin): matrices, bones, components,
  collisions, strings, game state, drawing and audio streams.
  `moonloader/lib/newopcodes.lua`, every function in
  [docs/newopcodes.md](docs/newopcodes.md).

## Install

Extract `moonsamp.zip` from the
[latest release](https://gitlab.com/akacross/moonsamp/-/releases) into the
GTA San Andreas folder, next to `gta_sa.exe`:

| File | |
| --- | --- |
| `moonsamp.asi` | the SA-MP API, the console and the opcodes |
| `moonsamp-LICENSE.txt` | GPL-3.0 |
| `moonloader/lib/moonsamp.lua` | the opcode runner the other two use |
| `moonloader/lib/newopcodes.lua` | NewOpcodes for scripts |
| `moonloader/lib/cleo5.lua` | CLEO 5's opcodes for scripts |

Rename `SAMPFUNCS.asi` to `SAMPFUNCS.asi.off`: moonsamp stays off while it
is there. CLEO is optional. Updating is the same: extract over the old
files. `moonsamp.log` in the game folder names the version, the SA-MP
build and the CLEO it found.

## Using the libs

MoonLoader cannot run an opcode it has no function for, so the CLEO 5 and
NewOpcodes opcodes come with a function each, named after the opcode
(`GET_CHAR_BONE_MATRIX` -> `getCharBoneMatrix`). Functions with a condition
return it first, then their outputs, as MoonLoader's own do:

```lua
local no = require 'newopcodes'
local cleo5 = require 'cleo5'

local ok, head = no.getCharBoneMatrix(PLAYER_PED, 8)
if ok then print(no.getMatrixPosition(head)) end

if cleo5.isKeyJustPressed(0x71) then cleo5.trace('F2 at %.1f', os.clock()) end

local ok, tex = no.loadTextureFromPngFile('mouse.png')
while true do                          -- drawing lasts one frame
    if ok then no.draw2dSprite(tex, 100, 100, 140, 140, 255, 255, 255, 255, 0) end
    no.printTemporaryText('hello', 40, 300, 0.6, 1.4, 1)
    wait(0)
end
```

- Handles are script handles; pointers are numbers.
- Audio stream functions in both libs take MoonLoader's AudioStream handles
  (or a BASS channel number): the object behind the handle has the channel
  at +8. `cleo5.audioStreamChannel(stream)` gives it.
- Formatted CLEO 5 functions (`trace`, `logToFile`, `displayTextFormatted`,
  ...) format in Lua with `string.format`.
- `M.available` is false when moonsamp.asi (or its `moonsamp_run` export)
  is not loaded; calls then raise an error.

`moonloader/lib/moonsamp.lua` is the runner both use, built on the
`moonsamp_run` export:

    int moonsamp_run(int op, const char *types, const int32_t *in, int nout, int32_t *out);

It writes the inputs in SCM format, one letter of `types` each (`i` an int,
`f` a float, `s` a string inline, or a `char *` past 255 bytes; `v` a local
variable read and written, `o` one only written, `e` the end of a variable
argument list), adds `nout` local variables for the outputs, calls
moonsamp's handler on a scratch script and copies the values out: the `v`
and `o` variables first, then the outputs. It returns the condition.
`require('moonsamp').call(op, inputs, outputs, cond, ...)` takes the same
letters plus `b` (a boolean) and `x` (a string or a number), and decodes
the outputs (`i u f b s`: int, unsigned, float, boolean, the text a
`char *` points at).

## SA-MP functions

GTA:SA 1.0 US. samp.dll 0.3.7-R1, R2, R3, R3-1, R4, R5 and 0.3.DL-R1; each
build's addresses and offsets are in `src/builds.h`.

| Opcode | Function |
| --- | --- |
| 0AF8 | sampAddChatMessage |
| 0AF9 | sampSendChat |
| 0AFA | isSampAvailable |
| 0B20 | sampGetCharHandleBySampPlayerId |
| 0B21 | sampIsChatInputActive |
| 0B23 | sampIsPlayerConnected |
| 0B2B | sampGetPlayerIdByCharHandle |
| 0B34 / 0B35 / 0B43 / 0B63 | sampRegisterChatCommand / its callback / sampUnregisterChatCommand |
| 0B36 | sampGetPlayerNickname |
| 0B37 | sampGetPlayerColor |
| 0B4F | sampGetGamestate |
| 0C8F | sampProcessChatInput |
| 0C90 | sampIsChatCommandDefined |
| 0B25 / 0B26 / 0B2A / 0B5C / 0B62 | sampGetPlayerHealth / Armor / Ping, sampIsPlayerPaused, sampGetPlayerSpecialAction |
| 0B57 | sampGetPlayerAnimationId (SA-MP's numbering) |
| 0B61 / 0C8A / 0C8B / 0B29 | sampIsLocalPlayerSpawned, sampGetMaxPlayerId, sampGetPlayerCount, sampSetLocalPlayerName |
| 0B2C / 0B46 / 0BB5 | sampGetVehicleIdByCarHandle, sampIs3dTextDefined, sampGetGangzonePoolPtr |
| 0B39 / 0B3A / 0B38 | sampGetCurrentServerAddress / Name, sampConnectToServer |
| 0BDC / 0B8C / 0B8D / 0BD5 | sampGetChatDisplayMode, sampIsCursorActive, sampSetCursorMode, sampIsScoreboardOpen |
| 0B3B / 0B3C / 0BD9 / 0BC9 | sampShowDialog, sampHasDialogRespond, sampSetDialogClientside, sampSendDialogResponse |
| 0B4C / 0B4D / 0B4E / 0B47 | sampIsDialogActive, sampGetCurrentDialogType / Id, sampCloseCurrentDialogWithButton |
| 0B48 / 0B49 / 0B4A / 0B4B | sampGet/SetCurrentDialogListItem, sampGet/SetCurrentDialogEditboxText |
| 0C49-0C5D | sampTextdrawIsExists, Get/SetString, Get/SetPos, Get/SetLetterSizeAndColor, Get/SetBoxEnabledColorAndSize, GetAlign, GetProportional, GetStyle, GetShadowColor, GetOutlineColor, GetModelRotationZoomVehColor |
| 0B64 / 0B65 / 0AFF / 0B2F | sampIsPlayerNpc, sampGetPlayerScore, sampGetCarHandleBySampVehicleId, sampGetStreamedOutPlayerPos |
| 0B58 / 0B59 | sampGetAnimationNameAndFile, sampFindAnimationIdByNameAndFile |
| 0B75 / 0B8E / 0BDA / 0BDB | sampGetChatString, sampGetCursorMode, sampIsDialogClientside, sampIsChatVisible |
| 0C46 | sampGet3dTextInfoById |
| 0BF8 / 0BF9 / 0C92 | raknetGetRpcName, raknetGetPacketName, getCleoLibraryVersion |
| 0B3D-0B40, 0BE7-0BF3 | raknetNewBitStream, Delete/Reset, Write*/Read*, offsets, sizes, data pointer |
| 0BF4 / 0BF5 | raknetBitStreamDecodeString / EncodeString (SA-MP's own StringCompressor) |
| 0BE0-0BE6 | the onSendRpc / onSendPacket / onReceiveRpc / onReceivePacket hooks (samp.events) |
| 0B41 / 0B42 / 0B8A / 0B8B | raknetSendRpc(Ex) / raknetSendBitStream(Ex) |
| 0BF6 / 0BF7 | raknetEmulRpcReceiveBitStream / raknetEmulPacketReceiveBitStream |
| 0BFC / 0BFD / 0C5F / 0C60 / 0C61 | Set/GetSampfuncsGlobalVar, isSampfuncsGlobalVarDefined, access (every script may read and write) |
| 0B78 / 0C7E | sampfuncsLog (to the console and moonsamp.log), isSampfuncsConsoleActive |
| 0C62 / 0C63 / 0C64 / 0C91 | runSampfuncsConsoleCommand, sampfuncsRegister/UnregisterConsoleCommand, isSampfuncsConsoleCommandDefined |
| 0C7F / 0C80 | command descriptions (console ones shown by `help`) |
| 0AF7, 0B7A-0B7F, 0BAC-0BB9, 0B24 | sampGetBase, RakClient / RakPeer and its functions and RPC nodes, every SA-MP object and pool pointer, sampGetPlayerStructPtr |
| 0B50-0B53 | object and pickup handles by SA-MP id and back |
| 0B54 / 0B5B / 0BD7 / 0BD8 | dialog listbox item count and text, dialog text and caption |
| 0B74 / 0B76 / 0B77 / 0B79 / 0BDD | sampSetChatString, chat input text (set / get), enable it, chat display mode |
| 0B5D / 0BD6 / 0B27 / 0B22 / 0B28 | sampToggleCursor, sampToggleScoreboard, sampSetGamestate, sampSetSendrate, sampDisconnectWithReason |
| 0AF6 / 0AFD | sampSpawnPlayer, sampSetSpecialAction (SA-MP's own CLocalPlayer functions) |
| 0AFB-0AFE, 0B30-0B33, 0BC8-0BD4 | the sampSend* RPCs, written as SA-MP's own senders write them |
| 0BBF, 0BC0-0BC7 | sampSendRconCommand, sampSend*Data (sync packets from the script's struct) |
| 0BBA-0BBE | sampStorePlayer*Data: the last sync data received from a player |
| 0C81-0C88 | sampForce*Sync: SA-MP's own senders, called now |
| 0C48, 0C4A-0C4F, 0C5E | sampTextdrawCreate, its setters, sampTextdrawDelete |
| 0B44 / 0C45 / 0B45 / 0C47 | sampCreate3dText(Ex), sampDestroy3dText, sampSet3dTextString |
| 0BDE / 0BDF, 0C6B-0C6F | pause / resume any script thread; threads running SCM code at a pointer, their locals |
| 0AAA / 0AB3 / 0AB4 | getScmThreadStructNamed, CLEO shared variables (without CLEO; with it, CLEO's own) |
| 0A92 / 0A94 | CLEO scripts: false without CLEO |
| 0AA5 | call_function (CLEO; used by the callback stubs) |
| 0B80-0B89, 0B90-0BAB | the dxut* script dialogs: buttons, checkboxes, edit boxes, statics, sliders, list boxes, DXUT's events |

Every opcode MoonLoader emits is covered. Any other opcode returns false and
leaves `""` / 0 in its results, and is named once in `moonsamp.log`.

RakNet hooks: RakClientInterface vtable slots Send (6), Receive (8),
DeallocatePacket (9) and RPC (25), and the call to RakPeer::HandleRPCPacket
inside RakClient::Receive (`handle_rpc_call` in `src/builds.h`). Hook
handlers get a copy of the stream: samp.dll frees its own BitStreams with
its own CRT.

A dialog a script shows is client-side: moonsamp shows it with
`m_bServerside = 2`, which SA-MP still treats as true, and the outgoing RPC
hook keeps its DialogResponse for sampHasDialogRespond instead of sending it.

The in-game self-test (`test/ingame/moonsamp_selftest.lua`, compared with
`test/ingame/compare.py`) runs the read-only functions under SAMPFUNCS and
under moonsamp; every value both can give matches.

## Console and dialogs

The console opens with `~` (Esc or `~` closes it) over the top left of the
screen. It shows moonloader.log as MoonLoader writes it, so it needs no
script, and what scripts send with sampfuncsLog; scriptHelper's copies of
moonloader.log (`[ML] ...`) are left out, so nothing shows twice. MoonLoader's
debug lines are hidden until `debug`. Typed lines run console commands;
`help` lists them, `clear` empties it. Up / Down recall typed lines, Tab
completes a command name, PgUp / PgDn and the wheel scroll. While open it
shows SA-MP's cursor, which holds the camera and controls, and keys held
when it opens are released, so a car rolls on instead of keeping its
throttle.

Script dialogs and the console are drawn on SA-MP's real Direct3D device
(`device` in `src/builds.h`), in its Present: after SA-MP's own UI. A dialog
takes the mouse only while SA-MP's cursor is shown, and the keyboard while
one of its edit boxes or list boxes has focus.

## CLEO 5's opcodes

moonsamp has CLEO 5's own opcodes (0DD5, 2000-2801; after CLEO 5's source,
MIT), for MoonLoader scripts with or without CLEO. Every function is in
[docs/cleo5.md](docs/cleo5.md):

- Input: keys just pressed, keys pressed in a range, key press and release
  emulation, the key bound to a game control, key names from the game's
  text. Key states are read once a frame while the game has the focus.
- Debug: trace, breakpoint and log to file. Trace and breakpoint go to the
  console and moonsamp.log; a breakpoint does not pause the game, since
  nothing would resume it.
- Files: open, close and size (CLEO's 0A9A-0A9C), positions, block reads
  and writes, write times. Paths take CLEO's `root:`, `user:`, `cleo:` and
  `modules:`; `.` is the moonloader folder, and anything else relative is
  the game folder's.
- Memory: copy, reads and writes at an offset, compare, the game's script
  list.
- Audio streams: speed, volume and speed transitions, progress, duration,
  type, source size (BASS channels).
- Text: tests, FXT files, formatted text labels, text on screen.
- Math and INI: bits, random picks, deleting INI sections and keys.

With CLEO none of these is registered with it (CLEO 5 has its own, and its
scripts expect those); the libs still run moonsamp's. Without CLEO, text
labels and FXT entries are served by a hook on CText::Get, installed with
the first one (over SA-MP's jump if it is there). With CLEO they go to
CLEO's own labels (0ADF / 0AE0): CLEO 5's hook puts the original bytes back
for each call it does not answer, which would take away a hook over its
own. 2000-2003 belong to CLEO's script functions, which MoonLoader scripts
do not have: no arguments, and only the condition is returned.

## NewOpcodes

moonsamp has all 126 opcodes of DK22Pac's NewOpcodes CLEO plugin (0D00-0D7F,
written anew from what they do). Every function is in
[docs/newopcodes.md](docs/newopcodes.md):

- Matrices, quaternions and vectors; ped bones; car components, atomics and
  materials; model alpha; collided entities; line of sight and colpoints;
  strings, memory and struct fields; the sun, weather, traffic lights, car
  lights, vehicle dummies, projectiles, volumes and the screen size.
- Drawing: 2D shapes and their vertices, sprites (plain, gradient, turned),
  texts in the game's fonts, temporary coronas, lights, temporary and
  permanent shadows, spotlights, and textures from TXD, PNG, BMP or DDS
  files.
- Audio streams (0D67-0D71): position in seconds, BASS_FX_DX8_* effects and
  their parameters, BASS_ATTRIB_* attributes and the 3D attributes.

As in NewOpcodes, a matrix, quat or vector of 1 to 10 is one of ten scratch
ones and colpoint 1 the scratch colpoint; a NULL one reads as zeroes.
Shape vertices of 1 are 20 scratch ones, else a pointer to 28-byte
vertices (x, y, z, rhw, ARGB colour, u, v). Sprite corners are in the
640x448 screen; text and shape positions are pixels. Textures are pointers
from `loadTextureFrom*File` or `findTextureInTxd*` (0 draws without one), and
entities for lights and coronas are pointers (`getCharPointer` and so on).

Drawing (`src/draw.c`): shapes, sprites and texts are queued and drawn once
a frame from Render2dStuff's empty call at 0x53E293 (plugin-sdk's drawing
event), under the HUD; spotlights from the effects pass (0x53E1E0). Both
hooks keep every register and go on to whatever call was there, and go in
with the first queued item. A frame the game does not draw (the menu, the
fade-in) drops what was queued for it, and freeing a texture drops queued
items that use it. Coronas (ids from 10000, the same each frame), shadows
and lights go straight to the game's lists. Textures stay until 0D7D.

Where NewOpcodes does not do what its opcodes say, moonsamp does: it never
installs its spotlight hook, gives permanent shadows a 1 ms life (its
arguments are out of order), turns a shadow's second axis by 90 radians,
and 0D41 writes to the scratch vertices whatever pointer it is given. Its
audio stream opcodes are empty stubs.

With CLEO, moonsamp registers none of them with it (CLEO 5 ships
NewOpcodes.cleo, whose handlers its scripts expect); the lib runs
moonsamp's.

## How MoonLoader talks to SAMPFUNCS

MoonLoader.asi does not link SAMPFUNCS. Every `samp*` / `raknet*` function is
a SAMPFUNCS opcode (0AF8-0C9x) that SAMPFUNCS registers with
`CLEO_RegisterOpcode`:

- MoonLoader writes the parameters in SCM format into a buffer (strings up to
  127 bytes as type 0x0E, longer ones as a `char*` in a type 0x01 int; results
  go to local var 0..n), points a CRunningScript at it and calls
  `CommandHandlerTable[op / 100](script, op)`. It reads the table address once
  at startup ("Initializing opcode handler table") from the operand at
  0x469FEE. ProcessOneCommand has its own copy at 0x469EF0; both are patched.
- `isSampfuncsLoaded()` is true when the byte at 0x561931 is `E9`;
  `isCleoLoaded()` checks 0x4667DB the same way. MoonLoader registers the
  samp* functions only when both are true at startup, so moonsamp sets both
  (each a jmp to a trampoline that runs the moved instruction).
- Callbacks (chat commands, RakNet hooks) are labels in a hidden MoonLoader
  script whose code is `0AA5 call <MoonLoader fn>` then a return opcode
  (`0B43` for commands, `0BE0` for hooks). SAMPFUNCS runs that code on the
  spot; so does moonsamp.

With CLEO:

- A callback stub's opcodes run on moonsamp's own handlers (0AA5 and the
  return), not through CLEO: MoonLoader runs opcodes on the stub's script
  while 0AA5 calls it, and CLEO 5 refuses an opcode on a script still inside
  one (silently, for a script it did not start). Anything else in a stub
  goes through the table at 0x469FEE: CLEO 4 replaces only that one, so
  ProcessOneCommand's copy at 0x469EF0 still has the game's 27 slots.
- CLEO 4 hooks 0x4667DB and so sets MoonLoader's CLEO marker; CLEO 5 does
  not, so moonsamp sets it whenever the byte is not already `E9`.
- CLEO 5 marks a script inside an opcode (bit 1 at +0xDF) and suspends one
  that starts another before the first ends. A hook can fire while its
  script is inside an opcode, so moonsamp clears the bit for the stub and
  puts it back after. Under CLEO 4 any bit of +0xDF means "a CLEO script",
  so it is left alone there.

## Diagnostics

`moonsamp.log` in the game folder starts with the version, the CLEO it
found, the SA-MP build and the hooks it put in (overlay, RakNet, and later
the CText::Get and drawing hooks when a script first needs them).

With a file named `moonsamp.trace` in the game folder, `moonsamp.log` also
records each opcode's answer per script when it changes, SA-MP's own state
behind the scripts' checks (chat box, dialog, scoreboard, cursor, spawned,
gamestate), every command SA-MP runs, chat messages, and keyboard input
reaching the game window with its focus. Delete the file to turn it off.

`moonsamp.log` never grows past 1 MB: when it would, it starts over, keeping
its first lines (version, SA-MP build, hooks) and a note that older lines
were dropped.

## Build and test

    ./build.sh          # -> moonsamp.asi (i686 mingw, no CRT)
    test/run.sh         # every test below, under Wine in a throwaway prefix
    test/run.sh "" samp-dlls/*/samp.dll    # the SA-MP tests on every build
    CLEO="cleo4/CLEO.asi cleo5/CLEO.asi" test/run.sh   # cleo_test beside each CLEO
    D3D9=path/to/dxvk/x32/d3d9.dll test/run.sh   # overlay_test on the GPU,
                        # where it also checks the overlay's frame time

`test/run.sh` runs moonsamp.asi against gta_sa.exe's own script engine (the
exe is only read; the copy that runs imports only KERNEL32) and a mapped,
not running, samp.dll with fake objects:

| Test | What it checks |
| --- | --- |
| `host_test.c` | the opcode host and chat commands |
| `newopcodes_test.c` | NewOpcodes through `moonsamp_run`, the drawing queue and its hooks (through a stand-in RW engine) |
| `cleo5_test.c` | CLEO 5's opcodes through `moonsamp_run` |
| `cleo_test.c` | moonsamp beside a real CLEO.asi: markers, table, callbacks, labels |
| `raknet_test.c` | BitStreams and the RakNet hooks |
| `phase2_test.c` | players, dialogs, textdraws, server |
| `overlay_test.c` | the console and dxut dialogs on a real D3D9 device |
| `newopcodes_lua_test.lua`, `cleo5_lua_test.lua` | the two libs under LuaJIT, and a function for every registered opcode |

It needs wine, python3, luajit and i686-w64-mingw32-gcc.

In-game checks (`test/ingame/`) cover what Wine cannot: the game's text,
keys, RenderWare, MoonLoader's audio streams and what draws on screen. Copy
one into `moonloader/tests/` and run it from Script-Helper's /sh menu; each
writes its result to `moonloader/logs/`:

| Script | |
| --- | --- |
| `moonsamp_selftest.lua` | the read-only SA-MP functions; run once under SAMPFUNCS and once under moonsamp, then `compare.py` the two logs |
| `newopcodes_check.lua` | RenderWare math, bones, car components, line of sight |
| `cleo5_check.lua` | key names, controller keys, labels, FXT, files, INI, memory, audio streams |
| `draw_check.lua` | 10 seconds of every kind of drawing, textures, NewOpcodes' audio streams |

Release: commit, tag `v<VERSION>`, push the tag, then
`python3 tools/release.py v<VERSION> "moonsamp <VERSION>" notes.md`. It builds
`dist/moonsamp.zip` from the tag (`tools/package.sh`), uploads it and creates
the GitLab release with a drop-in install link.

`tools/lib_docs.py <library>/sa/sa.json` writes `docs/newopcodes.md` and
`docs/cleo5.md` again from the libs, with names and links from
[Sanny Builder's library](https://github.com/sannybuilder/library).

`tools/find_offsets.py` locates SA-MP functions in other samp.dll builds from
their R3-1 addresses (relocation-masked signatures; `REF=<build>` uses another
build's). `tools/check_builds.py <dir>` derives every value in `src/builds.h`
again from each build's own code and reports any that differ (needs pefile
and capstone). R1 and R2 keep some functions in their protector's section,
where it cannot read them; it says which values those are.

## License

GPL-3.0, see LICENSE. The CLEO 5 opcodes follow CLEO 5's source (MIT); the
NewOpcodes ones are written anew from what DK22Pac's plugin does.
