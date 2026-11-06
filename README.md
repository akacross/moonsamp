# moonsamp

The SA-MP API for MoonLoader without SAMPFUNCS or CLEO. Drop `moonsamp.asi`
into the game folder; scripts run unchanged.

It stays off when `SAMPFUNCS.asi` is installed, and registers its opcodes
through CLEO when `CLEO.asi` is installed.

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

## Supported

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

Every opcode MoonLoader emits is covered.

The console opens with `~` (Esc or `~` closes it) over the top left of the
screen. It shows moonloader.log as MoonLoader writes it, so it needs no
script, and what scripts send with sampfuncsLog; scriptHelper's copies of
moonloader.log (`[ML] ...`) are left out, so nothing shows twice. MoonLoader's
debug lines are hidden until `debug`. Typed lines run console commands;
`help` lists them, `clear` empties it. Up / Down recall typed lines, Tab
completes a command name, PgUp / PgDn and the wheel scroll. While open it
shows SA-MP's cursor, which holds the camera and controls.

Script dialogs and the console are drawn on SA-MP's real Direct3D device
(`device` in `src/builds.h`), in its Present: after SA-MP's own UI. A dialog
takes the mouse only while SA-MP's cursor is shown, and the keyboard while
one of its edit boxes or list boxes has focus.

RakNet hooks: RakClientInterface vtable slots Send (6), Receive (8),
DeallocatePacket (9) and RPC (25), and the call to RakPeer::HandleRPCPacket
inside RakClient::Receive (`handle_rpc_call` in `src/builds.h`). Hook handlers get a copy of the stream: samp.dll
frees its own BitStreams with its own CRT.

A dialog a script shows is client-side: moonsamp shows it with
`m_bServerside = 2`, which SA-MP still treats as true, and the outgoing RPC
hook keeps its DialogResponse for sampHasDialogRespond instead of sending it.

The in-game self-test (`test/ingame/moonsamp_selftest.lua`, compared with
`test/ingame/compare.py`) runs the read-only functions under SAMPFUNCS and
under moonsamp; every value both can give matches.

Any other opcode returns false and leaves `""` / 0 in its results, and is
named once in `moonsamp.log`.

## Diagnostics

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
    test/run.sh         # opcode host, RakNet and the SA-MP opcodes under Wine,
                        # against gta_sa.exe's script engine and a mapped (not
                        # running) samp.dll with fake objects
    test/run.sh "" samp-dlls/*/samp.dll    # the SA-MP tests on every build
    D3D9=path/to/dxvk/x32/d3d9.dll test/run.sh   # overlay_test on the GPU,
                        # where it also checks the overlay's frame time

Release: commit, tag `v<VERSION>`, push the tag, then
`python3 tools/release.py v<VERSION> "moonsamp <VERSION>" notes.md`. It builds
`dist/moonsamp.zip` from the tag (`tools/package.sh`), uploads it and creates
the GitLab release with a drop-in install link.

`tools/find_offsets.py` locates SA-MP functions in other samp.dll builds from
their R3-1 addresses (relocation-masked signatures; `REF=<build>` uses another
build's). `tools/check_builds.py <dir>` derives every value in `src/builds.h`
again from each build's own code and reports any that differ (needs pefile
and capstone). R1 and R2 keep some functions in their protector's section,
where it cannot read them; it says which values those are.

## License

GPL-3.0, see LICENSE.
