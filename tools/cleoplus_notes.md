## Left out: already in MoonLoader, Lua or the other libs

| Opcodes | Use instead |
| --- | --- |
| 0B20, 0B21 `READ/WRITE_CLIPBOARD_DATA` | `getClipboardText`, `setClipboardText` |
| 0D01-0D59 (36 opcodes CLEO+ shares with NewOpcodes) | the same opcodes in [newopcodes.lua](newopcodes.md) |
| 0E0E `GET_CURRENT_RESOLUTION` | `getScreenResolution` |
| 0E10, 0E11 `IS_MOUSE_WHEEL_UP/DOWN` | `getMousewheelDelta` |
| 0E12 `GET_VEHICLE_SUBCLASS` | newopcodes `getVehicleClassAndSubclass` |
| 0E13 `GET_ENTITY_TYPE` | newopcodes `getEntityTypeAndClass` |
| 0E14-0E1C extended char, car and object vars | a Lua table keyed by handle |
| 0E20 `IS_ON_SAMP` | `isSampLoaded`, `isSampAvailable` |
| 0E21, 0E22 `GET_AUDIO_SFX/RADIO_VOLUME` | newopcodes `getGameVolume` |
| 0E27 `GET_ANGLE_FROM_TWO_COORDS` | `getHeadingFromVector2d` |
| 0E28, 0E6A, 0EE2, 0EE3 struct writes, `MAKE_NOP` | the `memory` library, newopcodes `getStructField` / `setStructField` |
| 0E3B `GET_AUDIOSTREAM_INTERNAL` | the audio functions in newopcodes.lua and cleo5.lua take MoonLoader's AudioStream handles |
| 0E3D `IS_KEY_JUST_PRESSED` | `wasKeyPressed`, cleo5 `isKeyJustPressed` |
| 0E3F `CONVERT_3D_TO_SCREEN_2D` | `convert3DCoordsToScreen` |
| 0E40, 0E41 `GET_CURRENT_HOUR/MINUTE` | `getTimeOfDay` |
| 0E45 `FRAME_MOD`, 0E4D `RANDOM_PERCENT` | Lua (`math.random`) |
| 0E4E-0E58 local onscreen timers and counters | need an SCM variable; MoonLoader scripts have none |
| 0E5B `GET_CAR_DUMMY_COORD` | newopcodes `getVehicleDummyPosn` |
| 0E5E `CHANGE_PLAYER_MONEY` | `givePlayerMoney`, `getPlayerMoney` |
| 0E6B `GET_COLPOINT_LIGHTING` | newopcodes `getColDataLighting` |
| 0E6F, 0E70 custom scripts from a label, 0ED0 `RETURN_SCRIPT_EVENT`, 0F0A `RETURN_TIMES` | SCM labels; not for Lua |
| 0E72-0E7E, 0F06-0F09 lists | Lua tables |
| 0E80, 0E81, 0EC2-0EC6 string buffers | Lua strings |
| 0E99 `LOAD_ALL_PRIORITY_MODELS_NOW` | `loadAllModelsNow` |
| 0EA7-0EA9 `GET_ANY_CHAR/CAR/OBJECT_NO_SAVE_RECURSIVE` | `getAllChars`, `getAllVehicles`, `getAllObjects` |
| 0EBE, 0EE4-0EED `LOCATE_*_DISTANCE_TO_*` | `getDistanceBetweenCoords3d` |
| 0EF0 `GET_COORD_FROM_ANGLED_DISTANCE`, 0EF3 `LERP`, 0EF4, 0EF7 `CLAMP_*` | Lua math |
| 0EFE `GET_LOADED_LIBRARY` | `getModuleHandle` |

## Not yet

Coming in later versions: script events as Lua callbacks (0ED1-0EDF, 0EE0,
0F0B, 0F0C), the drawing opcodes (0E1E, 0E62, 0E63), CLEO blips (0E2A,
0E2B), render objects (0E2E-0E31, 0E35-0E37, 0E3A, 0F02-0F04), special
models and characters (0E9A, 0E9B, 0F00, 0F01, 0F05), sprite textures
(0E3C), FX particles (0EBF-0EC1) and the second player (0EA0-0EA2).

## How some differ from CLEO+

- `ease` and the noise functions are plain Lua. `ease` modes 1-10:
  quadratic, cubic, quartic, quintic, sinusoidal, exponential, circular,
  elastic, back, bounce, any other linear; ways 0 in, 1 out, 2 in-out. `ease` fixes CLEO+'s slips:
  bounce out returned 0, quintic out overshot (1.168 at 0.3) and quintic
  in-out lost its first half, elastic missed 0 and 1 at the ends. The noise
  matches CLEO+'s SimplexNoise to within float rounding.
- The AI flags (0E44, 0E46-0E4C) walk the task trees when asked; CLEO+
  reads a copy it takes once a frame. For the player with no kill task,
  `getCharKillTargetChar` gives the ped the player targets (CLEO+ meant to,
  but never reached it).
- `setCameraControl` patches the camera's 33 mouse and stick reads as CLEO+
  does, but writes 0050FB12, the operand: CLEO+ writes 0050FB10, the start
  of the fmul there.
- `isCharReallyInAir` answers by the vehicle for a ped in one: a riding ped
  has no physics of its own, so CLEO+ calls it in the air.
- Damage (0EB5, 0EB6) counts for the frame it was taken and the next, and
  the damage hooks go in the first time a script asks for it.
- `getCurrentSaveSlot`, `isGameFirstStart` and `getModelNamePointer` hook
  the game at startup, like CLEO+.
