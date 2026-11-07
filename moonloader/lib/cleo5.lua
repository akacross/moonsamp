--[[ CLEO 5's own opcodes (0DD5, 2000-2801) for MoonLoader scripts, run by
moonsamp.asi with or without CLEO:

    local cleo5 = require 'cleo5'
    if cleo5.isKeyJustPressed(0x71) then
        cleo5.trace('F2 at %d', os.clock())
    end

A function per opcode, named after it (IS_KEY_JUST_PRESSED ->
isKeyJustPressed). Opcodes with a condition return it first, then their
outputs, as MoonLoader's own functions do. Keys are virtual key codes.
Paths take CLEO's root:, user:, cleo: and modules:; "." is the moonloader
folder and any other relative path the game folder's. Formatted texts are
formatted here with string.format. Audio stream functions take MoonLoader's
AudioStream handles (or a BASS channel). setBit, clearBit and toggleBit
return the changed value. openFile, closeFile and getFileSize (0A9A-0A9C)
give the files the file functions take. ]]
local core = require 'moonsamp'

local M = { available = core.available }

-- For tests: see moonsamp.lua.
function M._bind(fn)
    core._bind(fn)
    M.available = core.available
end

local call = core.call

local function def(name, op, inputs, outputs, cond)
    core.define(M, name, op, inputs, outputs, cond)
end

local function format(fmt, ...)
    if select('#', ...) == 0 then return tostring(fmt) end
    return tostring(fmt):format(...)
end

M.audioStreamChannel = core.channel

local function audio(name, op, inputs, outputs, cond)
    core.define_audio(M, name, op, inputs, outputs, cond)
end

-- core
def('getPlatform',                       0x0DD5, '', 'i')
def('getCleoArgCount',                   0x2000, '', 'i')
function M.cleoReturnWith(result, ...)
    local n = select('#', ...)
    return call(0x2002, 'b' .. ('i'):rep(n) .. 'e', '', true, result, ...)
end
function M.cleoReturnFail(...)
    local n = select('#', ...)
    return call(0x2003, ('i'):rep(n) .. 'e', '', true, ...)
end

-- input
def('isKeyJustPressed',                  0x2080, 'i', '', true)
def('getKeyPressedInRange',              0x2081, 'iio', 'i', true)     -- (min, max): found, key
def('getKeyJustPressedInRange',          0x2082, 'iio', 'i', true)
def('emulateKeyPress',                   0x2083, 'i', '')
def('emulateKeyRelease',                 0x2084, 'i', '')
def('getControllerKey',                  0x2085, 'iio', 'i', true)     -- (action, alt): found, key
def('getKeyName',                        0x2086, 'i', 's', true)

-- debug: to the moonsamp console and moonsamp.log
function M.breakpoint(blocking, fmt, ...)
    return call(0x2100, 'bsse', '', false, blocking, '%s', format(fmt or '', ...))
end
function M.trace(fmt, ...) return call(0x2101, 'sse', '', false, '%s', format(fmt, ...)) end
function M.logToFile(path, timestamp, fmt, ...)
    return call(0x2102, 'sbsse', '', false, path, timestamp, '%s', format(fmt, ...))
end

-- files
def('openFile',                          0x0A9A, 'ss', 'u', true)      -- (path, mode): ok, file
def('closeFile',                         0x0A9B, 'i', '')
def('getFileSize',                       0x0A9C, 'i', 'i')
def('getFilePosition',                   0x2300, 'i', 'i')
def('readBlockFromFile',                 0x2301, 'iii', '', true)      -- (file, size, buffer pointer)
def('writeBlockToFile',                  0x2302, 'iii', '', true)
def('resolveFilepath',                   0x2303, 's', 's')
def('getScriptFilename',                 0x2304, 'iio', 's', true)     -- only CLEO's scripts: false
def('getFileWriteTime',                  0x2305, 's', 'iiiiiii', true) -- y, m, d, h, min, s, ms

-- memory and scripts
def('copyMemory',                        0x2400, 'iii', '')            -- (src, dst, size)
def('readMemoryWithOffset',              0x2401, 'iii', 'i')           -- (address, offset, size)
def('writeMemoryWithOffset',             0x2402, 'iiii', '')           -- (address, offset, size, value)
def('forgetMemory',                      0x2403, 'i', '')
def('getScriptStructJustCreated',        0x2404, '', 'u')
def('isScriptRunning',                   0x2405, 'i', '', true)
def('getScriptStructFromFilename',       0x2406, 's', 'u', true)      -- (filename): only CLEO's scripts: false
def('isMemoryEqual',                     0x2407, 'iii', '', true)
def('terminateScript',                   0x2408, 'i', '')

-- audio streams
audio('isAudioStreamPlaying',            0x2500, 'i', '', true)
audio('getAudioStreamDuration',          0x2501, 'i', 'f')
audio('getAudioStreamSpeed',             0x2502, 'i', 'f')
audio('setAudioStreamSpeed',             0x2503, 'if', '')
audio('setAudioStreamVolumeWithTransition', 0x2504, 'ifi', '')         -- (stream, volume, ms)
audio('setAudioStreamSpeedWithTransition',  0x2505, 'ifi', '')
audio('setAudioStreamSourceSize',        0x2506, 'if', '')
audio('getAudioStreamProgress',          0x2507, 'i', 'f')
audio('setAudioStreamProgress',          0x2508, 'if', '')
audio('getAudioStreamType',              0x2509, 'i', 'i')
audio('setAudioStreamType',              0x250A, 'ii', '')
audio('getAudioStreamProgressSeconds',   0x250B, 'i', 'f')
audio('setAudioStreamProgressSeconds',   0x250C, 'if', '')

-- text
def('isTextEmpty',                       0x2600, 's', '', true)       -- (text)
def('isTextEqual',                       0x2601, 'ssb', '', true)      -- (a, b, ignoreCase)
def('isTextInText',                      0x2602, 'ssb', '', true)
def('isTextPrefix',                      0x2603, 'ssb', '', true)
def('isTextSuffix',                      0x2604, 'ssb', '', true)
function M.displayTextFormatted(x, y, fmt, ...)                        -- this frame only
    return call(0x2605, 'ffsse', '', false, x, y, '%s', format(fmt, ...))
end
def('loadFxt',                           0x2606, 's', '', true)
def('unloadFxt',                         0x2607, 's', '', true)
def('getTextLength',                     0x2608, 's', 'i')
function M.addTextLabelFormatted(label, fmt, ...)
    return call(0x2609, 'ssse', '', false, label, '%s', format(fmt, ...))
end

-- math
def('isBitSet',                          0x2700, 'ii', '', true)
def('setBit',                            0x2701, 'vi', 'i')            -- (value, bit): the new value
def('clearBit',                          0x2702, 'vi', 'i')           -- (value, bit): the new value
def('toggleBit',                         0x2703, 'vib', 'i')           -- (value, bit, state): the new value
function M.isTruthy(v)
    return call(0x2704, type(v) == 'string' and 's' or 'i', '', true, v)
end
function M.pickRandomInt(...)
    return call(0x2705, ('i'):rep(select('#', ...)) .. 'oe', 'i', false, ...)
end
function M.pickRandomFloat(...)
    return call(0x2706, ('f'):rep(select('#', ...)) .. 'oe', 'f', false, ...)
end
function M.pickRandomText(...)
    return call(0x2707, ('s'):rep(select('#', ...)) .. 'oe', 's', false, ...)
end
def('randomChance',                      0x2708, 'f', '', true)        -- (percent)

-- INI files
def('deleteSectionFromIniFile',          0x2800, 'ss', '', true)       -- (path, section)
def('deleteKeyFromIniFile',              0x2801, 'sss', '', true)      -- (path, section, key)

return M
