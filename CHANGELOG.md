# Changelog

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
