--[[
    CLEO 5 opcodes check: what the Wine tests cannot run (the game's text,
    keys, MoonLoader's audio streams), through moonloader/lib/cleo5.lua.

    Copy into moonloader/tests/ and run it from the /sh menu: once you are
    spawned it shows "cleo5 check" on screen for 5 seconds, writes
    moonloader/logs/cleo5_check.log and unloads. It plays a hit sound muted, writes and deletes a few files
    in moonloader/logs, and adds two text labels.
]]
script_name('cleo5_check')

local ffi = require 'ffi'
local out, passed, failed = {}, 0, 0

local function fmt(v)
    if type(v) == 'number' and math.floor(v) ~= v then return string.format('%.4f', v) end
    return tostring(v)
end

local function check(label, ok, ...)
    local detail = {}
    for i = 1, select('#', ...) do detail[#detail + 1] = fmt(select(i, ...)) end
    out[#out + 1] = string.format('CHECK %-52s %s  %s', label, ok and 'ok' or 'FAIL', table.concat(detail, ', '))
    if ok then passed = passed + 1 else failed = failed + 1 end
end

local function value(label, ...)
    local parts = {}
    for i = 1, select('#', ...) do parts[#parts + 1] = fmt(select(i, ...)) end
    out[#out + 1] = string.format('VALUE %-52s %s', label, table.concat(parts, ', '))
end

local function close(a, b, eps) return type(a) == 'number' and math.abs(a - b) < (eps or 0.01) end

local function run(c)
    local logs = getWorkingDirectory() .. '\\logs\\'
    check('getPlatform is PLATFORM_WINDOWS', c.getPlatform() == 9, c.getPlatform())

    -- input, from the game's own controls and text
    local ok, name = c.getKeyName(0x20)
    check('getKeyName of space, from the game text', ok and name and #name > 0, ok, name)
    value('getKeyName F5 / numpad 3 / left mouse', select(2, c.getKeyName(0x74)), select(2, c.getKeyName(0x63)),
        select(2, c.getKeyName(0x01)))
    local kok, key = c.getControllerKey(0, 0)
    check('getControllerKey fire (action 0) has a key', kok and key > 0, kok, key)
    value('getControllerKey sprint (action 16)', c.getControllerKey(16, 0))
    value('getKeyPressedInRange 1..254 now', c.getKeyPressedInRange(1, 254))

    -- text labels and FXT, read back through MoonLoader's getGxtText
    c.addTextLabelFormatted('MSCHK', 'label %d', 5)
    check('addTextLabelFormatted, read by getGxtText', getGxtText('MSCHK') == 'label 5', getGxtText('MSCHK'))
    local f = io.open(logs .. 'cleo5_check.fxt', 'w')
    f:write('# test\nMSFXT checked text\n')
    f:close()
    check('loadFxt from the moonloader folder', c.loadFxt('.\\logs\\cleo5_check.fxt') and getGxtText('MSFXT') == 'checked text',
        getGxtText('MSFXT'))
    check('unloadFxt', c.unloadFxt('.\\logs\\cleo5_check.fxt'))
    check('a game text still reads after the hook', #getGxtText('FEC_SPC') > 0, getGxtText('FEC_SPC'))
    os.remove(logs .. 'cleo5_check.fxt')
    check('isTextInText ignoring case', c.isTextInText('MoonSamp', 'nsa', true))

    -- files
    local path = c.resolveFilepath('.\\logs\\cleo5_check.bin')
    check('resolveFilepath . is the moonloader folder', path:lower() == (logs .. 'cleo5_check.bin'):lower(), path)
    local fok, h = c.openFile(path, 'wb')
    local data = ffi.new('char[6]', 'moon!')
    check('openFile, writeBlockToFile', fok and c.writeBlockToFile(h, 5, tonumber(ffi.cast('uintptr_t', data))), fok, h)
    check('getFilePosition', c.getFilePosition(h) == 5, c.getFilePosition(h))
    c.closeFile(h)
    local back = ffi.new('char[6]')
    fok, h = c.openFile(path, 'rb')
    check('readBlockFromFile reads it back', fok and c.readBlockFromFile(h, 5, tonumber(ffi.cast('uintptr_t', back)))
        and ffi.string(back) == 'moon!', ffi.string(back))
    c.closeFile(h)
    local wok, y = c.getFileWriteTime(path)
    check('getFileWriteTime', wok and y >= 2024, wok, y)
    os.remove(path)

    -- INI
    local ini = logs .. 'cleo5_check.ini'
    f = io.open(ini, 'w')
    f:write('[a]\nk1=1\nk2=2\n[b]\nk=3\n')
    f:close()
    check('deleteKeyFromIniFile', c.deleteKeyFromIniFile(ini, 'a', 'k1'))
    check('deleteSectionFromIniFile', c.deleteSectionFromIniFile(ini, 'b'))
    f = io.open(ini, 'r')
    local text = f:read('*a')
    f:close()
    check('the INI file keeps the rest', not text:find('k1') and text:find('k2') and not text:find('%[b%]'), (text:gsub('\n', ' ')))
    os.remove(ini)

    -- log to file
    c.logToFile(logs .. 'cleo5_check_file.log', false, 'n=%d', 42)
    f = io.open(logs .. 'cleo5_check_file.log', 'r')
    text = f and f:read('*a') or ''
    if f then f:close() end
    check('logToFile', text:find('n=42') ~= nil, (text:gsub('\r?\n', ' ')))
    os.remove(logs .. 'cleo5_check_file.log')
    c.trace('cleo5 check trace %d', 1)

    -- memory, math
    local a, b = ffi.new('uint8_t[8]', 1, 2, 3, 4), ffi.new('uint8_t[8]')
    c.copyMemory(tonumber(ffi.cast('uintptr_t', a)), tonumber(ffi.cast('uintptr_t', b)), 8)
    check('copyMemory, isMemoryEqual', c.isMemoryEqual(tonumber(ffi.cast('uintptr_t', a)), tonumber(ffi.cast('uintptr_t', b)), 8))
    check('setBit', c.setBit(1, 4) == 17, c.setBit(1, 4))
    local pick = c.pickRandomInt(3, 5, 7)
    check('pickRandomInt', pick == 3 or pick == 5 or pick == 7, pick)

    -- MoonLoader's audio stream
    local sound = getWorkingDirectory() .. '\\resource\\audio\\hitsounds\\p1_bell.mp3'
    local stream = doesFileExist(sound) and loadAudioStream(sound)
    if stream then
        setAudioStreamVolume(stream, 0)
        setAudioStreamState(stream, 1)
        wait(200)
        local ch = c.audioStreamChannel(stream)
        local mlLength = getAudioStreamLength(stream)
        check('audioStreamChannel finds the BASS channel', ch and ch ~= 0, ch)
        check('isAudioStreamPlaying', c.isAudioStreamPlaying(stream))
        local dur = c.getAudioStreamDuration(stream)
        check('getAudioStreamDuration = getAudioStreamLength', close(dur, mlLength, 0.05), dur, mlLength)
        c.setAudioStreamSpeed(stream, 2)
        check('setAudioStreamSpeed / getAudioStreamSpeed', close(c.getAudioStreamSpeed(stream), 2), c.getAudioStreamSpeed(stream))
        check('the duration halves at speed 2', close(c.getAudioStreamDuration(stream), mlLength / 2, 0.05),
            c.getAudioStreamDuration(stream))
        local p = c.getAudioStreamProgress(stream)
        check('getAudioStreamProgress moves', p > 0 and p <= 1, p)
        c.setAudioStreamProgress(stream, 0.5)
        check('setAudioStreamProgress', close(c.getAudioStreamProgress(stream), 0.5, 0.05), c.getAudioStreamProgress(stream))
        c.setAudioStreamType(stream, 2)
        check('setAudioStreamType / getAudioStreamType', c.getAudioStreamType(stream) == 2)
        setAudioStreamState(stream, 0)
        releaseAudioStream(stream)
    else
        out[#out + 1] = 'SKIP  no hit sound to load: the audio stream checks did not run'
    end

    -- on screen for 5 seconds
    local t = os.clock()
    while os.clock() - t < 5 do
        c.displayTextFormatted(40, 300, 'cleo5 check: %d passed, %d failed', passed, failed)
        wait(0)
    end
end

function main()
    while not isPlayerPlaying(PLAYER_HANDLE) do wait(100) end
    wait(3000)
    local ok, c = pcall(require, 'cleo5')
    if not ok then
        out[#out + 1] = 'FAIL  require cleo5: ' .. tostring(c)
        failed = failed + 1
    elseif not c.available then
        out[#out + 1] = 'FAIL  cleo5: moonsamp.asi has no moonsamp_run'
        failed = failed + 1
    else
        local good, err = pcall(run, c)
        if not good then
            out[#out + 1] = 'FAIL  error: ' .. tostring(err)
            failed = failed + 1
        end
    end
    table.insert(out, 1, string.format('cleo5 check: %d passed, %d failed', passed, failed))
    local f = io.open(getWorkingDirectory() .. '\\logs\\cleo5_check.log', 'w')
    if f then
        f:write(table.concat(out, '\n'), '\n')
        f:close()
    end
    sampAddChatMessage(string.format('[cleo5 check] %d passed, %d failed: logs\\cleo5_check.log', passed, failed), 0xFFFFFF)
    thisScript():unload()
end
