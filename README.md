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

GTA:SA 1.0 US. samp.dll 0.3.7-R3-1 and 0.3.DL-R1.

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
| 0B57 | sampGetPlayerAnimationId (the ped's strongest-blended animation, GTA's numbering) |
| 0B61 / 0C8A / 0C8B / 0B29 | sampIsLocalPlayerSpawned, sampGetMaxPlayerId, sampGetPlayerCount, sampSetLocalPlayerName |
| 0B2C / 0B46 / 0BB5 | sampGetVehicleIdByCarHandle, sampIs3dTextDefined, sampGetGangzonePoolPtr |
| 0B39 / 0B3A / 0B38 | sampGetCurrentServerAddress / Name, sampConnectToServer |
| 0BDC / 0B8C / 0B8D / 0BD5 | sampGetChatDisplayMode, sampIsCursorActive, sampSetCursorMode, sampIsScoreboardOpen |
| 0B3B / 0B3C / 0BD9 / 0BC9 | sampShowDialog, sampHasDialogRespond, sampSetDialogClientside, sampSendDialogResponse |
| 0B4C / 0B4D / 0B4E / 0B47 | sampIsDialogActive, sampGetCurrentDialogType / Id, sampCloseCurrentDialogWithButton |
| 0B48 / 0B49 / 0B4A / 0B4B | sampGet/SetCurrentDialogListItem, sampGet/SetCurrentDialogEditboxText |
| 0C49-0C5D | sampTextdrawIsExists, Get/SetString, Get/SetPos, Get/SetLetterSizeAndColor, Get/SetBoxEnabledColorAndSize |
| 0B3D-0B40, 0BE7-0BF3 | raknetNewBitStream, Delete/Reset, Write*/Read*, offsets, sizes, data pointer |
| 0BF4 / 0BF5 | raknetBitStreamDecodeString / EncodeString (SA-MP's own StringCompressor) |
| 0BE0-0BE6 | the onSendRpc / onSendPacket / onReceiveRpc / onReceivePacket hooks (samp.events) |
| 0B41 / 0B42 / 0B8A / 0B8B | raknetSendRpc(Ex) / raknetSendBitStream(Ex) |
| 0BF6 / 0BF7 | raknetEmulRpcReceiveBitStream / raknetEmulPacketReceiveBitStream |
| 0BFC / 0BFD / 0C5F / 0C60 / 0C61 | Set/GetSampfuncsGlobalVar, isSampfuncsGlobalVarDefined, access (every script may read and write) |
| 0B78 / 0C7E | sampfuncsLog (to moonsamp.log), isSampfuncsConsoleActive (always false: no console window) |
| 0C62 / 0C63 / 0C64 / 0C91 | runSampfuncsConsoleCommand, sampfuncsRegister/UnregisterConsoleCommand, isSampfuncsConsoleCommandDefined |
| 0C7F / 0C80 | command descriptions (accepted, unused) |
| 0AA5 | call_function (CLEO; used by the callback stubs) |

RakNet hooks: RakClientInterface vtable slots Send (6), Receive (8),
DeallocatePacket (9) and RPC (25), and the call to RakPeer::HandleRPCPacket
inside RakClient::Receive (R3-1 `0x408E7`, DL `0x40AE7`; also found in R1
`0x3D537` and R5 `0x41027`). Hook handlers get a copy of the stream: samp.dll
frees its own BitStreams with its own CRT.

A dialog a script shows is client-side: moonsamp shows it with
`m_bServerside = 2`, which SA-MP still treats as true, and the outgoing RPC
hook keeps its DialogResponse for sampHasDialogRespond instead of sending it.

Any other opcode returns false and leaves `""` / 0 in its results, and is
named once in `moonsamp.log`.

## Diagnostics

With a file named `moonsamp.trace` in the game folder, `moonsamp.log` also
records each opcode's answer per script when it changes, SA-MP's own state
behind the scripts' checks (chat box, dialog, scoreboard, cursor, spawned,
gamestate), every command SA-MP runs, chat messages, and keyboard input
reaching the game window with its focus. Delete the file to turn it off.

## Build and test

    ./build.sh          # -> moonsamp.asi (i686 mingw, no CRT)
    test/run.sh         # opcode host, RakNet and the SA-MP opcodes under Wine,
                        # against gta_sa.exe's script engine and a mapped (not
                        # running) samp.dll with fake objects

Release: commit, tag `v<VERSION>`, push the tag, then
`python3 tools/release.py v<VERSION> "moonsamp <VERSION>" notes.md`. It builds
`dist/moonsamp.zip` from the tag (`tools/package.sh`), uploads it and creates
the GitLab release with a drop-in install link.

`tools/find_offsets.py` locates SA-MP functions in other samp.dll builds from
their R3-1 addresses (relocation-masked signatures).

## License

GPL-3.0, see LICENSE.
