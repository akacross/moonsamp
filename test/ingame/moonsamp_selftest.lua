--[[
    moonsamp self-test: MoonLoader's SA-MP functions, read-only.

    Copy into moonloader/, play, and it writes
    moonloader/logs/moonsamp_selftest-<provider>.log once you are spawned
    (provider: "sampfuncs" or "moonsamp"). Run it once with SAMPFUNCS and once
    with moonsamp, then compare the two logs (test/ingame/compare.py).

    Nothing is sent to the server and nothing is shown: only getters, a
    BitStream battery and a SAMPFUNCS global variable. Sections:
      CHECK  must hold under either provider (exact)
      VALUE  a snapshot; differences between sessions are expected for
             dynamic values (ping, health, positions)
]]
script_name('moonsamp_selftest')

local out, provider = {}, 'unknown'

local function fmt(v)
    local t = type(v)
    if t == 'number' then
        if v ~= v then return 'nan' end
        if math.floor(v) == v and math.abs(v) < 2^53 then return string.format('%d', v) end
        return string.format('%.4f', v)
    elseif t == 'string' then
        return string.format('%q', v)
    elseif t == 'nil' then
        return 'nil'
    end
    return tostring(v)
end

local function pack(...) return { n = select('#', ...), ... } end

-- call(name, ...) -> "r1, r2, ..." or "nil-fn" / "error: ..."
local function call(name, ...)
    local f = _G[name]
    if type(f) ~= 'function' then return 'nil-fn' end
    local r = pack(pcall(f, ...))
    if not r[1] then return 'error: ' .. tostring(r[2]):gsub('\n.*', '') end
    local parts = {}
    for i = 2, r.n do parts[#parts + 1] = fmt(r[i]) end
    return table.concat(parts, ', ')
end

local function value(label, name, ...)
    out[#out + 1] = string.format('VALUE %-48s %s', label, call(name, ...))
end

local function check(label, ok, detail)
    out[#out + 1] = string.format('CHECK %-48s %s%s', label, ok and 'ok' or 'FAIL',
        detail and ('  (' .. detail .. ')') or '')
end

local function get(name, ...)
    local f = _G[name]
    if type(f) ~= 'function' then return nil end
    local r = pack(pcall(f, ...))
    if not r[1] then return nil end
    return unpack(r, 2, r.n)
end

---------------------------------------------------------------------------

local function bitstreams()
    local bs = get('raknetNewBitStream')
    check('raknetNewBitStream returns a stream', type(bs) == 'number' and bs ~= 0)
    if not bs or bs == 0 then return end
    raknetBitStreamWriteBool(bs, true)
    raknetBitStreamWriteInt8(bs, 0xAB)
    raknetBitStreamWriteInt16(bs, 0xBEEF)
    raknetBitStreamWriteInt32(bs, -123456789)
    raknetBitStreamWriteFloat(bs, 1.5)
    raknetBitStreamWriteString(bs, 'moonsamp')
    value('bits used after bool,i8,i16,i32,float,8 chars', 'raknetBitStreamGetNumberOfBitsUsed', bs)
    value('bytes used', 'raknetBitStreamGetNumberOfBytesUsed', bs)
    value('write offset', 'raknetBitStreamGetWriteOffset', bs)
    check('read bool', get('raknetBitStreamReadBool', bs) == true)
    local i8, i16, i32 = get('raknetBitStreamReadInt8', bs), get('raknetBitStreamReadInt16', bs), get('raknetBitStreamReadInt32', bs)
    check('read int8 (unsigned)', i8 == 0xAB, fmt(i8))
    check('read int16 (unsigned)', i16 == 0xBEEF, fmt(i16))
    check('read int32 (signed)', i32 == -123456789, fmt(i32))
    local f = get('raknetBitStreamReadFloat', bs)
    check('read float', f == 1.5, fmt(f))
    local s = get('raknetBitStreamReadString', bs, 8)
    check('read string', s == 'moonsamp', fmt(s))
    value('unread bits at the end', 'raknetBitStreamGetNumberOfUnreadBits', bs)
    raknetBitStreamResetReadPointer(bs)
    raknetBitStreamIgnoreBits(bs, 1)
    check('ignore 1 bit, read int8 again', get('raknetBitStreamReadInt8', bs) == 0xAB)
    raknetBitStreamSetReadOffset(bs, 9)
    check('set read offset 9, read int16', get('raknetBitStreamReadInt16', bs) == 0xBEEF)
    value('read offset after that', 'raknetBitStreamGetReadOffset', bs)
    raknetBitStreamSetWriteOffset(bs, 9)
    raknetBitStreamWriteInt16(bs, 0x1234)
    value('bits used after rewriting at 9', 'raknetBitStreamGetNumberOfBitsUsed', bs)
    raknetBitStreamResetReadPointer(bs)
    raknetBitStreamIgnoreBits(bs, 9)
    check('rewritten int16 reads back', get('raknetBitStreamReadInt16', bs) == 0x1234)
    check('data pointer is set', (get('raknetBitStreamGetDataPtr', bs) or 0) ~= 0)

    local other = raknetNewBitStream()
    raknetBitStreamWriteInt32(other, 7)
    raknetBitStreamWriteBitStream(bs, other)
    value('bits after appending a 32-bit stream', 'raknetBitStreamGetNumberOfBitsUsed', bs)
    raknetDeleteBitStream(other)

    local enc = raknetNewBitStream()
    raknetBitStreamEncodeString(enc, 'Hello, moonsamp! 123')
    value('encoded "Hello, moonsamp! 123": bits', 'raknetBitStreamGetNumberOfBitsUsed', enc)
    raknetBitStreamResetReadPointer(enc)
    local dec = get('raknetBitStreamDecodeString', enc, 64)
    check('encode / decode round trip', dec == 'Hello, moonsamp! 123', fmt(dec))
    raknetDeleteBitStream(enc)

    raknetResetBitStream(bs)
    value('bits after reset', 'raknetBitStreamGetNumberOfBitsUsed', bs)
    raknetDeleteBitStream(bs)
    value('raknetGetRpcName(25)', 'raknetGetRpcName', 25)
    value('raknetGetPacketName(207)', 'raknetGetPacketName', 207)
end

local function globals()
    local name = 'moonsamp_selftest_var'
    check('setSampfuncsGlobalVar', get('setSampfuncsGlobalVar', name, 4242) == true)
    local r = pack(get('getSampfuncsGlobalVar', name))
    check('getSampfuncsGlobalVar returns it', r[1] == true and r[2] == 4242, fmt(r[1]) .. ', ' .. fmt(r[2]))
    check('isSampfuncsGlobalVarDefined', get('isSampfuncsGlobalVarDefined', name) == true)
    check('an undefined var is not defined', get('isSampfuncsGlobalVarDefined', 'moonsamp_no_such_var') == false)
    value('getSampfuncsGlobalVarAccessForThread', 'getSampfuncsGlobalVarAccessForThread', name, 0)
end

local function state()
    value('isSampLoaded', 'isSampLoaded')
    value('isSampfuncsLoaded', 'isSampfuncsLoaded')
    value('isCleoLoaded', 'isCleoLoaded')
    value('isSampAvailable', 'isSampAvailable')
    value('sampGetGamestate', 'sampGetGamestate')
    value('sampGetCurrentServerName', 'sampGetCurrentServerName')
    value('sampGetCurrentServerAddress', 'sampGetCurrentServerAddress')
    value('sampGetChatDisplayMode', 'sampGetChatDisplayMode')
    value('sampIsChatInputActive', 'sampIsChatInputActive')
    value('sampIsChatVisible', 'sampIsChatVisible')
    value('sampGetChatInputText', 'sampGetChatInputText')
    value('sampGetChatString(99)', 'sampGetChatString', 99)
    value('sampIsDialogActive', 'sampIsDialogActive')
    value('sampGetCurrentDialogId', 'sampGetCurrentDialogId')
    value('sampGetCurrentDialogType', 'sampGetCurrentDialogType')
    value('sampIsDialogClientside', 'sampIsDialogClientside')
    value('sampIsScoreboardOpen', 'sampIsScoreboardOpen')
    value('sampIsCursorActive', 'sampIsCursorActive')
    value('sampGetCursorMode', 'sampGetCursorMode')
    value('isSampfuncsConsoleActive', 'isSampfuncsConsoleActive')
    value('getScreenResolution', 'getScreenResolution')
    value('sampIsLocalPlayerSpawned', 'sampIsLocalPlayerSpawned')
    value('sampGetMaxPlayerId(false)', 'sampGetMaxPlayerId', false)
    value('sampGetMaxPlayerId(true)', 'sampGetMaxPlayerId', true)
    value('sampGetPlayerCount(false)', 'sampGetPlayerCount', false)
    value('sampGetPlayerCount(true)', 'sampGetPlayerCount', true)
    value('getCleoLibraryVersion', 'getCleoLibraryVersion')
    check('sampGetGangzonePoolPtr is set', (get('sampGetGangzonePoolPtr') or 0) ~= 0)
    for _, p in ipairs{ 'sampGetSampInfoPtr', 'sampGetSampPoolsPtr', 'sampGetPlayerPoolPtr', 'sampGetVehiclePoolPtr',
                        'sampGetTextdrawPoolPtr', 'sampGetChatInfoPtr', 'sampGetInputInfoPtr', 'sampGetDialogInfoPtr' } do
        check(p .. ' is set', (get(p) or 0) ~= 0)
    end
    for _, c in ipairs{ 'ab', 'fastmap', 'sh', 'moonsamp_no_such_cmd' } do
        value('sampIsChatCommandDefined(' .. c .. ')', 'sampIsChatCommandDefined', c)
    end
    value('isSampfuncsConsoleCommandDefined(lua)', 'isSampfuncsConsoleCommandDefined', 'lua')
end

local function players()
    -- counted here, next to sampGetPlayerCount, to see what each provider counts
    local top0 = get('sampGetMaxPlayerId', false) or 0
    local connected, npcs, streamed = 0, 0, 0
    for id = 0, top0 do
        if get('sampIsPlayerConnected', id) then
            connected = connected + 1
            if get('sampIsPlayerNpc', id) then npcs = npcs + 1 end
            if get('sampGetCharHandleBySampPlayerId', id) then streamed = streamed + 1 end
        end
    end
    out[#out + 1] = string.format('VALUE %-48s %d connected, %d NPCs, %d streamed', 'counted remote players', connected, npcs, streamed)
    local ok, me = sampGetPlayerIdByCharHandle(PLAYER_PED)
    check('local player id from PLAYER_PED', ok == true, fmt(me))
    local ids = { me }
    local top = get('sampGetMaxPlayerId', false) or 0
    for id = 0, top do
        if #ids >= 8 then break end
        if id ~= me and get('sampIsPlayerConnected', id) then ids[#ids + 1] = id end
    end
    out[#out + 1] = 'VALUE players sampled                                     ' .. #ids
    for n, id in ipairs(ids) do
        local who = n == 1 and 'local' or ('remote#' .. (n - 1))
        value(who .. ' sampIsPlayerConnected', 'sampIsPlayerConnected', id)
        value(who .. ' sampGetPlayerNickname', 'sampGetPlayerNickname', id)
        value(who .. ' sampGetPlayerColor', 'sampGetPlayerColor', id)
        value(who .. ' sampGetPlayerPing', 'sampGetPlayerPing', id)
        value(who .. ' sampGetPlayerScore', 'sampGetPlayerScore', id)
        value(who .. ' sampIsPlayerNpc', 'sampIsPlayerNpc', id)
        value(who .. ' sampGetPlayerHealth', 'sampGetPlayerHealth', id)
        value(who .. ' sampGetPlayerArmor', 'sampGetPlayerArmor', id)
        value(who .. ' sampIsPlayerPaused', 'sampIsPlayerPaused', id)
        value(who .. ' sampGetPlayerSpecialAction', 'sampGetPlayerSpecialAction', id)
        value(who .. ' sampGetPlayerAnimationId', 'sampGetPlayerAnimationId', id)
        value(who .. ' sampGetStreamedOutPlayerPos', 'sampGetStreamedOutPlayerPos', id)
        local hok, handle = get('sampGetCharHandleBySampPlayerId', id)
        value(who .. ' sampGetCharHandleBySampPlayerId (found)', 'sampGetCharHandleBySampPlayerId', id)
        if hok then
            local bok, back = sampGetPlayerIdByCharHandle(handle)
            check(who .. ' ped handle -> player id round trip', bok == true and back == id, fmt(back))
            check(who .. ' ped handle is a live ped', doesCharExist(handle))
        end
    end
end

local function vehicles()
    local n = 0
    for _, car in ipairs(getAllVehicles()) do
        if n >= 6 then break end
        n = n + 1
        local ok, id = get('sampGetVehicleIdByCarHandle', car)
        out[#out + 1] = string.format('VALUE %-48s %s', 'vehicle#' .. n .. ' sampGetVehicleIdByCarHandle', fmt(ok) .. ', ' .. fmt(id))
        if ok then
            local bok, back = get('sampGetCarHandleBySampVehicleId', id)
            check('vehicle#' .. n .. ' id -> car handle round trip', bok == true and back == car, fmt(bok) .. ', ' .. fmt(back))
        end
    end
    out[#out + 1] = 'VALUE vehicles sampled                                    ' .. n
end

local function textdraws()
    local count, sampled = 0, 0
    for id = 0, 2303 do
        if get('sampTextdrawIsExists', id) then
            count = count + 1
            if sampled < 10 then
                sampled = sampled + 1
                local w = 'textdraw ' .. id
                value(w .. ' string', 'sampTextdrawGetString', id)
                value(w .. ' pos', 'sampTextdrawGetPos', id)
                value(w .. ' letter size, colour', 'sampTextdrawGetLetterSizeAndColor', id)
                value(w .. ' box', 'sampTextdrawGetBoxEnabledColorAndSize', id)
                value(w .. ' align', 'sampTextdrawGetAlign', id)
                value(w .. ' proportional', 'sampTextdrawGetProportional', id)
                value(w .. ' style', 'sampTextdrawGetStyle', id)
                value(w .. ' shadow, colour', 'sampTextdrawGetShadowColor', id)
                value(w .. ' outline, colour', 'sampTextdrawGetOutlineColor', id)
                value(w .. ' model, rotation, zoom, colours', 'sampTextdrawGetModelRotationZoomVehColor', id)
            end
        end
    end
    out[#out + 1] = 'VALUE textdraws existing                                  ' .. count
    local labels = 0
    for id = 0, 2047 do
        if get('sampIs3dTextDefined', id) then
            labels = labels + 1
            if labels <= 5 then value('3D text ' .. id .. ' info', 'sampGet3dTextInfoById', id) end
        end
    end
    out[#out + 1] = 'VALUE 3D texts defined                                    ' .. labels
end

local function write()
    local path = getWorkingDirectory() .. '\\logs\\moonsamp_selftest-' .. provider .. '.log'
    local f = io.open(path, 'w')
    if not f then return end
    f:write('moonsamp self-test, provider ', provider, ', ', os.date('%Y-%m-%d %H:%M:%S'), '\n')
    for _, line in ipairs(out) do f:write(line, '\n') end
    f:close()
    sampAddChatMessage('[moonsamp self-test] written: logs\\moonsamp_selftest-' .. provider .. '.log', 0xFFFFFF)
end

function main()
    if not isSampLoaded() then return end
    while not isSampAvailable() do wait(200) end
    local ffi = require 'ffi'
    pcall(ffi.cdef, 'void* GetModuleHandleA(const char*);')
    local function loaded(name) return ffi.C.GetModuleHandleA(name) ~= nil end   -- NULL cdata is truthy
    if loaded('SAMPFUNCS.asi') then provider = 'sampfuncs'
    elseif loaded('moonsamp.asi') then provider = 'moonsamp'
    else provider = 'none' end
    while not sampIsLocalPlayerSpawned() do wait(500) end
    wait(5000)
    for _, section in ipairs{ state, globals, bitstreams, players, vehicles, textdraws } do
        local ok, err = pcall(section)
        if not ok then out[#out + 1] = 'ERROR section: ' .. tostring(err) end
    end
    write()
end
