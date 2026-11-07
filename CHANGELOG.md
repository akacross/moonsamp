# Changelog

## 0.10.0

CLEO 5's own opcodes for MoonLoader scripts, with or without CLEO. The
in-game check passes 28 of 28 with no CLEO and beside CLEO 5.5.0.

**Added**

- 66 of CLEO 5's opcodes (0DD5, 2000-2801): input (keys just pressed, keys
  in a range, key emulation, controller keys, key names), debug output
  (trace, breakpoint, log to file), files (0A9A-0A9C, positions, blocks,
  CLEO's virtual paths, write times), memory (copy, offset reads and
  writes, compare, the game's script list), audio streams, text (tests,
  FXT files, formatted labels, text on screen), math (bits, random picks)
  and INI deletes.
- `moonloader/lib/cleo5.lua`, in the zip: a function per opcode, named
  after it (`isKeyJustPressed`, `trace`, `setAudioStreamSpeed`, ...). It
  takes MoonLoader's AudioStream handles for the audio opcodes.
- `moonloader/lib/moonsamp.lua`, in the zip: the `moonsamp_run` runner both
  libs use.

**Changed**

- With CLEO none of these is registered with it, so CLEO 5 keeps its own;
  text labels and FXT entries then go to CLEO's labels. Without CLEO
  moonsamp hooks CText::Get for them, with the first one.
- `newopcodes.lua` is built on `moonsamp.lua`.

## 0.9.0

MoonLoader scripts beside CLEO 5, which SAMPFUNCS does not run with, and
fixes beside CLEO 4. Checked in game with no CLEO, CLEO 4.4.4 and CLEO 5.5.0
(self-test 90 of 90 with CLEO 5).

**Added**

- CLEO 5: MoonLoader registers its SA-MP functions (moonsamp sets the CLEO
  marker CLEO 5 leaves alone), and chat commands, console commands and
  RakNet hooks run.
- CLEO.asi is found in `scripts\` and `plugins\` too, where CLEO 5.5 may
  sit; moonsamp.log names the CLEO version.

**Fixed**

- With CLEO 4, every callback (chat and console commands, RakNet hooks)
  would have crashed: CLEO 4 replaces only one of the game's two opcode
  table references, and stubs ran through the other.
- With CLEO, moonsamp no longer registers its NewOpcodes with CLEO, so
  NewOpcodes.cleo (shipped with CLEO 5) keeps its own; `newopcodes.lua`
  uses moonsamp's.

## 0.8.0

95 of NewOpcodes' opcodes, and a MoonLoader lib to call them. The in-game
check passes 22 of 22.

**Added**

- NewOpcodes (DK22Pac's CLEO plugin, 0D00-0D7F) without CLEO: matrices,
  quaternions, vectors, ped bones, car components and atomics, model alpha,
  collided entities, line of sight and colpoints, strings, memory and
  struct fields, the sun, weather, traffic lights, car lights, vehicle
  dummies, projectiles, volumes and the screen size. Drawing and the CLEO
  audio stream effects are not there yet.
- `moonloader/lib/newopcodes.lua`, in the zip: a function per opcode, named
  after it (`getCharBoneMatrix`, `getCarComponent`, ...), run through
  moonsamp's new `moonsamp_run` export.
- Unlike NewOpcodes, a bad handle or a NULL matrix gives false or zeroes
  instead of a crash, and car lights are read only on automobiles.

## 0.7.2

Opening the console no longer leaves keys held down.

**Fixed**

- Keys held when the console opens, or when a dxut edit box or list box
  takes the keyboard, are released in the game: a car coasts instead of
  driving on, and nothing stays pressed after the console closes.

## 0.7.1

The console and dxut dialogs no longer slow the game down.

**Fixed**

- The console and dxut dialogs cost a twelfth of what they did each frame
  (0.15 ms with a full console and a dialog on DXVK, from 1.8 ms): text is
  drawn from a glyph texture made once, in one draw call a frame, instead
  of D3DX laying out every string again, and only the states moonsamp
  changes are saved and put back. d3dx9_43.dll is no longer needed.

## 0.7.0

Every SA-MP 0.3.7 build and every opcode MoonLoader emits, with
SAMPFUNCS's console and dxut dialogs. The in-game self-test passes 90 of 90.

**Added**

- SA-MP 0.3.7-R1, R2, R3, R4 and R5, next to R3-1 and 0.3.DL-R1: every
  function moonsamp has works on all seven builds.
- `tools/check_builds.py` checks every address and offset moonsamp uses
  against each build's own code.
- SAMPFUNCS's console, opened with ~: moonloader.log as MoonLoader writes
  it (no script needed; scriptHelper's copies are left out), sampfuncsLog
  lines, console commands with history, Tab completion and `help`.
- The dxut* script dialogs (0B80-0BAB): buttons, checkboxes, edit boxes,
  statics, sliders and list boxes with DXUT's events. Every opcode
  MoonLoader emits is now covered.
- Every other SAMPFUNCS function (104 more): SA-MP object and
  pool pointers, object and pickup handles, listbox items, chat input and
  lines, cursor, scoreboard, game state, send rates; spawning and special
  actions; every sampSend* RPC and sync packet, stored and forced sync
  data; creating textdraws and 3D texts; script threads; CLEO's shared
  variables and getScmThreadStructNamed when CLEO is not installed.

**Fixed**

- `raknetEmulRpcReceiveBitStream` handed SA-MP the RakClientInterface
  where it needs the RakPeer 0xDDE below it, so SA-MP looked up the RPC
  on the wrong object.
- `sampGetListboxItemText` returned an empty string: MoonLoader passes a
  buffer for the text, which moonsamp now fills.

## 0.6.1

Checked against SAMPFUNCS with an in-game self-test run under both
(`test/ingame/`): every comparable value now matches.

**Fixed**

- `sampGetPlayerPing` was always 0: SA-MP only asks the server for scores
  and pings while the scoreboard is open. moonsamp asks every 3 s, as
  SAMPFUNCS keeps them current.
- `sampIsPlayerPaused` was true for players not streamed in.
- `sampGetCharHandleBySampPlayerId` gives no handle for the local player, as
  SAMPFUNCS does.
- `sampGetPlayerAnimationId` uses SA-MP's animation numbering (your own from
  SA-MP's current animation, other players' from their sync), not GTA's.
- Textdraw colours are ARGB to scripts (SA-MP keeps ABGR), in getters and
  setters.
- Unimplemented functions with more than 8 results no longer read a stale
  pointer as a string (`sampGet3dTextInfoById` crashed the script that way).

**Added**

- `sampGetPlayerScore`, `sampIsPlayerNpc`, `sampGetCarHandleBySampVehicleId`,
  `sampGetStreamedOutPlayerPos`, `sampGetCursorMode`, `sampIsChatVisible`,
  `sampIsDialogClientside`, `sampGetChatString`, `getCleoLibraryVersion`.
- `raknetGetRpcName`, `raknetGetPacketName` (names from the SAMPFUNCS SDK).
- `sampGetAnimationNameAndFile`, `sampFindAnimationIdByNameAndFile`.
- `sampGet3dTextInfoById`; textdraw align, proportional, style, shadow,
  outline and model getters.

## 0.5.0

- Chat colours: `sampAddChatMessage` colours are converted for SA-MP, which
  takes RGBA. Every colour but white came out wrong in 0.4.9 (pink showed as
  green).
- `moonsamp.log` never grows past 1 MB. When it would, it starts over,
  keeping its first lines and a note that older lines were dropped.
- The trace no longer logs every RakNet callback, only failed ones.

## 0.4.9

First release.

**What it does**

- Runs MoonLoader's `samp*` and `raknet*` functions without SAMPFUNCS or
  CLEO. moonsamp registers the same script opcodes SAMPFUNCS does, so scripts
  run unchanged, `samp.events` included.
- Stays off when `SAMPFUNCS.asi` is in the game folder, so it can be left
  installed next to it. With `CLEO.asi` installed, it registers its opcodes
  through CLEO.
- GTA San Andreas 1.0 US, MoonLoader 026, SA-MP 0.3.7-R3-1 and 0.3.DL-R1.

**Covered**

- Chat: send, add messages, chat commands and their parameters, the chat box
  and its display mode.
- Players: connected, nickname, colour (with its alpha), ped and vehicle
  handles, health, armour, ping, paused, special action, animation, max id,
  count, local player spawned and name.
- Dialogs: show, respond, read and set the list item and editbox, close;
  client-side dialogs answered without reaching the server.
- Textdraws: exists, string, position, letter size and colour, box.
- Server name, address, gamestate, reconnect; cursor and scoreboard.
- RakNet: BitStreams, the four RPC and packet hooks, sending, emulating
  received RPCs and packets, SA-MP's string compression.
- SAMPFUNCS global variables, shared by every script; console commands
  (there is no console window: `isSampfuncsConsoleActive()` is false and
  `sampfuncsLog` writes to `moonsamp.log`).
- `getScreenResolution`, which MoonLoader routes through SAMPFUNCS once it
  is loaded.

**Not covered yet**

- SA-MP 0.3.7-R1 and R5 are recognised but get no SA-MP functions.
- Any other SAMPFUNCS opcode returns false and empty results, and is named
  once in `moonsamp.log`.

**Diagnostics**

- `moonsamp.log` next to the ASI. A file named `moonsamp.trace` in the game
  folder adds a detailed trace (see the README).
