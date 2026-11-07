# cleo5.lua

Every function in `moonloader/lib/cleo5.lua`: CLEO 5's own opcodes for MoonLoader scripts, run by moonsamp.asi with or without CLEO. `local cleo5 = require 'cleo5'`.

Opcodes link to their page in [Sanny Builder's library](https://library.sannybuilder.com/#/sa); parameter names follow it. Functions with a condition return it first (`ok`), then their outputs.

## Core

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0DD5](https://library.sannybuilder.com/#/sa/script/extensions/CLEO/0DD5) | `getPlatform()` | platform | `GET_PLATFORM` |  |
| [2000](https://library.sannybuilder.com/#/sa/script/extensions/CLEO/2000) | `getCleoArgCount()` | count | `GET_CLEO_ARG_COUNT` |  |
| [2002](https://library.sannybuilder.com/#/sa/script/extensions/CLEO/2002) | `cleoReturnWith(result, ...)` | ok | `CLEO_RETURN_WITH` |  |
| [2003](https://library.sannybuilder.com/#/sa/script/extensions/CLEO/2003) | `cleoReturnFail(...)` | ok | `CLEO_RETURN_FAIL` |  |

## Input

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [2080](https://library.sannybuilder.com/#/sa/script/extensions/input/2080) | `isKeyJustPressed(keyCode)` | ok | `IS_KEY_JUST_PRESSED` |  |
| [2081](https://library.sannybuilder.com/#/sa/script/extensions/input/2081) | `getKeyPressedInRange(min, max)` | found, key | `GET_KEY_PRESSED_IN_RANGE` |  |
| [2082](https://library.sannybuilder.com/#/sa/script/extensions/input/2082) | `getKeyJustPressedInRange(minKeyCode, maxKeyCode)` | ok, keyCode | `GET_KEY_JUST_PRESSED_IN_RANGE` |  |
| [2083](https://library.sannybuilder.com/#/sa/script/extensions/input/2083) | `emulateKeyPress(keyCode)` |  | `EMULATE_KEY_PRESS` |  |
| [2084](https://library.sannybuilder.com/#/sa/script/extensions/input/2084) | `emulateKeyRelease(keyCode)` |  | `EMULATE_KEY_RELEASE` |  |
| [2085](https://library.sannybuilder.com/#/sa/script/extensions/input/2085) | `getControllerKey(action, alt)` | found, key | `GET_CONTROLLER_KEY` |  |
| [2086](https://library.sannybuilder.com/#/sa/script/extensions/input/2086) | `getKeyName(keyCode)` | ok, name | `GET_KEY_NAME` |  |

## Debug: to the moonsamp console and moonsamp.log

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [2100](https://library.sannybuilder.com/#/sa/script/extensions/debug/2100) | `breakpoint(blocking, fmt, ...)` |  | `BREAKPOINT` |  |
| [2101](https://library.sannybuilder.com/#/sa/script/extensions/debug/2101) | `trace(fmt, ...)` |  | `TRACE` |  |
| [2102](https://library.sannybuilder.com/#/sa/script/extensions/debug/2102) | `logToFile(path, timestamp, fmt, ...)` |  | `LOG_TO_FILE` |  |

## Files

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0A9A](https://library.sannybuilder.com/#/sa/script/extensions/CLEO/0A9A) | `openFile(path, mode)` | ok, file | `OPEN_FILE` |  |
| [0A9B](https://library.sannybuilder.com/#/sa/script/extensions/CLEO/0A9B) | `closeFile(file)` |  | `CLOSE_FILE` |  |
| [0A9C](https://library.sannybuilder.com/#/sa/script/extensions/CLEO/0A9C) | `getFileSize(file)` | size | `GET_FILE_SIZE` |  |
| [2300](https://library.sannybuilder.com/#/sa/script/extensions/file/2300) | `getFilePosition(file)` | position | `GET_FILE_POSITION` |  |
| [2301](https://library.sannybuilder.com/#/sa/script/extensions/file/2301) | `readBlockFromFile(file, size, buffer pointer)` | ok | `READ_BLOCK_FROM_FILE` |  |
| [2302](https://library.sannybuilder.com/#/sa/script/extensions/file/2302) | `writeBlockToFile(file, size, address)` | ok | `WRITE_BLOCK_TO_FILE` |  |
| [2303](https://library.sannybuilder.com/#/sa/script/extensions/file/2303) | `resolveFilepath(path)` | resolved | `RESOLVE_FILEPATH` |  |
| [2304](https://library.sannybuilder.com/#/sa/script/extensions/file/2304) | `getScriptFilename(address, fullPath)` | ok, filenameOrPath | `GET_SCRIPT_FILENAME` | only CLEO's scripts: false |
| [2305](https://library.sannybuilder.com/#/sa/script/extensions/file/2305) | `getFileWriteTime(fileName)` | ok, year, month, day, hour, minute, second, milisecond | `GET_FILE_WRITE_TIME` | y, m, d, h, min, s, ms |

## Memory and scripts

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [2400](https://library.sannybuilder.com/#/sa/script/extensions/memory/2400) | `copyMemory(src, dst, size)` |  | `COPY_MEMORY` |  |
| [2401](https://library.sannybuilder.com/#/sa/script/extensions/memory/2401) | `readMemoryWithOffset(address, offset, size)` | result | `READ_MEMORY_WITH_OFFSET` |  |
| [2402](https://library.sannybuilder.com/#/sa/script/extensions/memory/2402) | `writeMemoryWithOffset(address, offset, size, value)` |  | `WRITE_MEMORY_WITH_OFFSET` |  |
| [2403](https://library.sannybuilder.com/#/sa/script/extensions/memory/2403) | `forgetMemory(address)` |  | `FORGET_MEMORY` |  |
| [2404](https://library.sannybuilder.com/#/sa/script/extensions/memory/2404) | `getScriptStructJustCreated()` | address | `GET_SCRIPT_STRUCT_JUST_CREATED` |  |
| [2405](https://library.sannybuilder.com/#/sa/script/extensions/memory/2405) | `isScriptRunning(address)` | ok | `IS_SCRIPT_RUNNING` |  |
| 2406 | `getScriptStructFromFilename(filename)` | ok, pointer |  | only CLEO's scripts: false |
| [2407](https://library.sannybuilder.com/#/sa/script/extensions/memory/2407) | `isMemoryEqual(addressA, addressB, size)` | ok | `IS_MEMORY_EQUAL` |  |
| [2408](https://library.sannybuilder.com/#/sa/script/extensions/memory/2408) | `terminateScript(address)` |  | `TERMINATE_SCRIPT` |  |

## Audio streams

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [2500](https://library.sannybuilder.com/#/sa/script/extensions/audio/2500) | `isAudioStreamPlaying(stream)` | ok | `IS_AUDIO_STREAM_PLAYING` |  |
| [2501](https://library.sannybuilder.com/#/sa/script/extensions/audio/2501) | `getAudioStreamDuration(stream)` | seconds | `GET_AUDIO_STREAM_DURATION` |  |
| [2502](https://library.sannybuilder.com/#/sa/script/extensions/audio/2502) | `getAudioStreamSpeed(stream)` | speed | `GET_AUDIO_STREAM_SPEED` |  |
| [2503](https://library.sannybuilder.com/#/sa/script/extensions/audio/2503) | `setAudioStreamSpeed(stream, speed)` |  | `SET_AUDIO_STREAM_SPEED` |  |
| [2504](https://library.sannybuilder.com/#/sa/script/extensions/audio/2504) | `setAudioStreamVolumeWithTransition(stream, volume, ms)` |  | `SET_AUDIO_STREAM_VOLUME_WITH_TRANSITION` |  |
| [2505](https://library.sannybuilder.com/#/sa/script/extensions/audio/2505) | `setAudioStreamSpeedWithTransition(stream, speed, timeMs)` |  | `SET_AUDIO_STREAM_SPEED_WITH_TRANSITION` |  |
| [2506](https://library.sannybuilder.com/#/sa/script/extensions/audio/2506) | `setAudioStreamSourceSize(stream, radius)` |  | `SET_AUDIO_STREAM_SOURCE_SIZE` |  |
| [2507](https://library.sannybuilder.com/#/sa/script/extensions/audio/2507) | `getAudioStreamProgress(stream)` | progress | `GET_AUDIO_STREAM_PROGRESS` |  |
| [2508](https://library.sannybuilder.com/#/sa/script/extensions/audio/2508) | `setAudioStreamProgress(stream, progress)` |  | `SET_AUDIO_STREAM_PROGRESS` |  |
| [2509](https://library.sannybuilder.com/#/sa/script/extensions/audio/2509) | `getAudioStreamType(stream)` | type | `GET_AUDIO_STREAM_TYPE` |  |
| [250A](https://library.sannybuilder.com/#/sa/script/extensions/audio/250A) | `setAudioStreamType(stream, type)` |  | `SET_AUDIO_STREAM_TYPE` |  |
| [250B](https://library.sannybuilder.com/#/sa/script/extensions/audio/250B) | `getAudioStreamProgressSeconds(stream)` | progress | `GET_AUDIO_STREAM_PROGRESS_SECONDS` |  |
| [250C](https://library.sannybuilder.com/#/sa/script/extensions/audio/250C) | `setAudioStreamProgressSeconds(stream, progress)` |  | `SET_AUDIO_STREAM_PROGRESS_SECONDS` |  |

## Text

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [2600](https://library.sannybuilder.com/#/sa/script/extensions/text/2600) | `isTextEmpty(text)` | ok | `IS_TEXT_EMPTY` |  |
| [2601](https://library.sannybuilder.com/#/sa/script/extensions/text/2601) | `isTextEqual(a, b, ignoreCase)` | ok | `IS_TEXT_EQUAL` |  |
| [2602](https://library.sannybuilder.com/#/sa/script/extensions/text/2602) | `isTextInText(text, subText, ignoreCase)` | ok | `IS_TEXT_IN_TEXT` |  |
| [2603](https://library.sannybuilder.com/#/sa/script/extensions/text/2603) | `isTextPrefix(text, prefix, ignoreCase)` | ok | `IS_TEXT_PREFIX` |  |
| [2604](https://library.sannybuilder.com/#/sa/script/extensions/text/2604) | `isTextSuffix(text, suffix, ignoreCase)` | ok | `IS_TEXT_SUFFIX` |  |
| [2605](https://library.sannybuilder.com/#/sa/script/extensions/text/2605) | `displayTextFormatted(x, y, fmt, ...)` |  | `DISPLAY_TEXT_FORMATTED` | this frame only |
| [2606](https://library.sannybuilder.com/#/sa/script/extensions/text/2606) | `loadFxt(filepath)` | ok | `LOAD_FXT` |  |
| [2607](https://library.sannybuilder.com/#/sa/script/extensions/text/2607) | `unloadFxt(filepath)` | ok | `UNLOAD_FXT` |  |
| [2608](https://library.sannybuilder.com/#/sa/script/extensions/text/2608) | `getTextLength(text)` | length | `GET_TEXT_LENGTH` |  |
| [2609](https://library.sannybuilder.com/#/sa/script/extensions/text/2609) | `addTextLabelFormatted(label, fmt, ...)` |  | `ADD_TEXT_LABEL_FORMATTED` |  |

## Math

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [2700](https://library.sannybuilder.com/#/sa/script/extensions/math/2700) | `isBitSet(number, bitIndex)` | ok | `IS_BIT_SET` |  |
| [2701](https://library.sannybuilder.com/#/sa/script/extensions/math/2701) | `setBit(value, bit)` | the new value | `SET_BIT` |  |
| [2702](https://library.sannybuilder.com/#/sa/script/extensions/math/2702) | `clearBit(value, bit)` | the new value | `CLEAR_BIT` |  |
| [2703](https://library.sannybuilder.com/#/sa/script/extensions/math/2703) | `toggleBit(value, bit, state)` | the new value | `TOGGLE_BIT` |  |
| [2704](https://library.sannybuilder.com/#/sa/script/extensions/math/2704) | `isTruthy(v)` | ok | `IS_TRUTHY` |  |
| [2705](https://library.sannybuilder.com/#/sa/script/extensions/math/2705) | `pickRandomInt(...)` | result | `PICK_RANDOM_INT` |  |
| [2706](https://library.sannybuilder.com/#/sa/script/extensions/math/2706) | `pickRandomFloat(...)` | result | `PICK_RANDOM_FLOAT` |  |
| [2707](https://library.sannybuilder.com/#/sa/script/extensions/math/2707) | `pickRandomText(...)` | result | `PICK_RANDOM_TEXT` |  |
| [2708](https://library.sannybuilder.com/#/sa/script/extensions/math/2708) | `randomChance(percent)` | ok | `RANDOM_CHANCE` |  |

## INI files

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [2800](https://library.sannybuilder.com/#/sa/script/extensions/ini/2800) | `deleteSectionFromIniFile(path, section)` | ok | `DELETE_SECTION_FROM_INI_FILE` |  |
| [2801](https://library.sannybuilder.com/#/sa/script/extensions/ini/2801) | `deleteKeyFromIniFile(path, section, key)` | ok | `DELETE_KEY_FROM_INI_FILE` |  |
