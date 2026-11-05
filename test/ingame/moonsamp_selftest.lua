--[[
    moonsamp self-test: MoonLoader's SA-MP functions, read-only.

    Copy into moonloader/, play, and it writes
    moonloader/logs/moonsamp_selftest-<provider>.log once you are spawned
    (provider: "sampfuncs" or "moonsamp"). Run it once with SAMPFUNCS and once
    with moonsamp, then compare the two logs (test/ingame/compare.py).

    Nothing is sent to the server but one on-foot and one stats sync (what
    the client sends anyway). The rest reads, or changes only this client
    and undoes it: a textdraw, a 3D text and a chat line made and removed, a
    script thread started and destroyed. Sections:
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
    raknetBitStreamSetWriteOffset(bs, 8)
    raknetBitStreamWriteInt16(bs, 0x1234)
    value('bits used after rewriting at 8', 'raknetBitStreamGetNumberOfBitsUsed', bs)
    raknetBitStreamResetReadPointer(bs)
    raknetBitStreamIgnoreBits(bs, 8)
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

local function more()
    local function set(name) local v = get(name) return type(v) == 'number' and v ~= 0 end
    for _, p in ipairs{ 'sampGetBase', 'sampGetRakclientInterface', 'sampGetRakpeer', 'sampGetServerSettingsPtr',
                        'sampGetKillInfoPtr', 'sampGetMiscInfoPtr', 'sampGetObjectPoolPtr', 'sampGetTextlabelPoolPtr',
                        'sampGetPickupPoolPtr' } do
        check(p .. ' is set', set(p))
    end
    check('sampGetRakpeer is 0xDDE below the RakClient', get('sampGetRakpeer') == get('sampGetRakclientInterface') - 0xDDE)
    check('RakClient vtable slot 6 is set', (get('sampGetRakclientFuncAddressByIndex', 6) or 0) ~= 0)
    check('RPC 61 (ShowDialog) has a node', (get('sampGetRpcNodeByRpcId', 61) or 0) ~= 0)
    check('RPC 61 has a handler', (get('sampGetRpcCallbackByRpcId', 61) or 0) ~= 0)
    value('getScmThreadStructNamed(MAIN) is set', 'getScmThreadStructNamed', 'MAIN')
    setCleoSharedVar(7, 1234)
    check('CLEO shared var round trip', get('getCleoSharedVar', 7) == 1234)

    local _, me = sampGetPlayerIdByCharHandle(PLAYER_PED)
    for id = 0, (get('sampGetMaxPlayerId', false) or 0) do
        if id ~= me and get('sampIsPlayerConnected', id) then
            check('sampGetPlayerStructPtr of a remote player is set', (get('sampGetPlayerStructPtr', id) or 0) ~= 0)
            break
        end
    end

    local objects, pickups, objbad, pickbad = 0, 0, 0, 0
    for id = 0, 2099 do
        local h = get('sampGetObjectHandleBySampId', id)
        if h and h ~= -1 and h ~= 0 then
            objects = objects + 1
            if get('sampGetObjectSampIdByHandle', h) ~= id then objbad = objbad + 1 end
        end
    end
    for id = 0, 4095 do
        local h = get('sampGetPickupHandleBySampId', id)
        if h and h ~= -1 and h ~= 0 then
            pickups = pickups + 1
            if get('sampGetPickupSampIdByHandle', h) ~= id then pickbad = pickbad + 1 end
        end
    end
    out[#out + 1] = string.format('VALUE %-48s %d', 'objects with a handle', objects)
    out[#out + 1] = string.format('VALUE %-48s %d', 'pickups with a handle', pickups)
    check('object id <-> handle round trips', objbad == 0, objbad .. ' bad')
    check('pickup id <-> handle round trips', pickbad == 0, pickbad .. ' bad')

    value('sampGetDialogCaption', 'sampGetDialogCaption')
    value('sampGetListboxItemsCount', 'sampGetListboxItemsCount')
    value('sampGetListboxItemText(0)', 'sampGetListboxItemText', 0)

    -- a chat line, changed and put back
    local text, prefix, color, pcolor = sampGetChatString(99)
    sampSetChatString(99, 'moonsamp self-test line', '[ST]', 0xFF00FF00, 0xFFFF0000)
    local t2, p2, c2, pc2 = sampGetChatString(99)
    check('sampSetChatString reads back', t2 == 'moonsamp self-test line' and p2 == '[ST]' and c2 == 0xFF00FF00 and pc2 == 0xFFFF0000,
          fmt(t2) .. ' ' .. fmt(p2) .. ' ' .. fmt(c2) .. ' ' .. fmt(pc2))
    sampSetChatString(99, text, prefix, color, pcolor)

    -- a textdraw on a free id, changed through every setter, deleted
    local td
    for id = 2303, 2200, -1 do if not get('sampTextdrawIsExists', id) then td = id break end end
    if td then
        sampTextdrawCreate(td, 'moonsamp', 600.0, 400.0)
        check('sampTextdrawCreate: it exists', get('sampTextdrawIsExists', td) == true)
        check('sampTextdrawCreate: its text', get('sampTextdrawGetString', td) == 'moonsamp')
        local x, y = get('sampTextdrawGetPos', td)
        check('sampTextdrawCreate: its position', x == 600.0 and y == 400.0, fmt(x) .. ', ' .. fmt(y))
        value('new textdraw letter size, colour', 'sampTextdrawGetLetterSizeAndColor', td)
        value('new textdraw box', 'sampTextdrawGetBoxEnabledColorAndSize', td)
        value('new textdraw align / proportional / style', 'sampTextdrawGetAlign', td)
        sampTextdrawSetAlign(td, 3)
        sampTextdrawSetProportional(td, 0)
        sampTextdrawSetStyle(td, 2)
        sampTextdrawSetShadow(td, 1, 0xFF112233)
        sampTextdrawSetOutlineColor(td, 2, 0xFF445566)
        sampTextdrawSetModelRotationZoomVehColor(td, 411, 1.0, 2.0, 3.0, 0.5, 6, 1)
        check('sampTextdrawSetAlign 3', get('sampTextdrawGetAlign', td) == 3)
        check('sampTextdrawSetProportional 0', get('sampTextdrawGetProportional', td) == 0)
        check('sampTextdrawSetStyle 2', get('sampTextdrawGetStyle', td) == 2)
        local o, oc = get('sampTextdrawGetOutlineColor', td)
        check('sampTextdrawSetOutlineColor', o == 2 and oc == 0xFF445566, fmt(o) .. ', ' .. fmt(oc))
        local m, rx, ry, rz, z, c1, c2 = get('sampTextdrawGetModelRotationZoomVehColor', td)
        check('sampTextdrawSetModelRotationZoomVehColor', m == 411 and ry == 2.0 and z == 0.5 and c1 == 6 and c2 == 1)
        sampTextdrawDelete(td)
        check('sampTextdrawDelete: gone', get('sampTextdrawIsExists', td) == false)
    end

    -- a 3D text far away, renamed, destroyed
    local lid = get('sampCreate3dText', 'moonsamp label', 0xFF00FF00, 0.0, 0.0, -100.0, 5.0, true, -1, -1)
    check('sampCreate3dText gives an id', type(lid) == 'number' and lid >= 0, fmt(lid))
    if type(lid) == 'number' and lid >= 0 then
        check('the 3D text is defined', get('sampIs3dTextDefined', lid) == true)
        value('new 3D text info', 'sampGet3dTextInfoById', lid)
        sampSet3dTextString(lid, 'renamed')
        check('sampSet3dTextString reads back', get('sampGet3dTextInfoById', lid) == 'renamed')
        sampDestroy3dText(lid)
        check('sampDestroy3dText: gone', get('sampIs3dTextDefined', lid) == false)
    end

    -- a script thread waiting forever on our own code: locals, pause, destroy
    local ffi = require 'ffi'
    local code = ffi.new('uint8_t[16]', { 0x01, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x7F })   -- wait 0x7FFFFFFF ms
    local th = get('createScmThreadAtPointer', tonumber(ffi.cast('uintptr_t', code)), { 5, 6 })
    check('createScmThreadAtPointer gives a thread', type(th) == 'number' and th ~= 0, fmt(th))
    if type(th) == 'number' and th ~= 0 then
        check('its arguments are its locals', get('getScmThreadLocalVar', th, 0) == 5 and get('getScmThreadLocalVar', th, 1) == 6)
        setScmThreadLocalVar(th, 3, 77)
        check('setScmThreadLocalVar / get', get('getScmThreadLocalVar', th, 3) == 77)
        pauseScmThread(th)
        resumeScmThread(th)
        wait(100)
        destroyScmThread(th)
        wait(100)
        check('the thread was destroyed without trouble', true)
    end
    _G.__selftest_code = code

    sampForceOnfootSync()
    sampForceStatsSync()
    check('sampForceOnfootSync / sampForceStatsSync ran', true)
end

-- A dxut dialog, hidden from the start and deleted at the end.
local function dxut()
    local d = get('dxutCreateDialog', 'moonsamp self-test')
    check('dxutCreateDialog gives a dialog', d ~= nil and d ~= 0)
    if not d or d == 0 then return end
    dxutSetDialogVisible(d, false)
    check('dxutIsDialogExists', get('dxutIsDialogExists', d) == true)
    check('dxutIsDialogVisible false after hiding', get('dxutIsDialogVisible', d) == false)
    dxutSetDialogPos(d, 100, 120, 300, 200)
    local x, y, w, h = get('dxutGetDialogPosAndSize', d)
    check('dxutSetDialogPos / GetDialogPosAndSize', x == 100 and y == 120 and w == 300 and h == 200,
        string.format('%s %s %s %s', tostring(x), tostring(y), tostring(w), tostring(h)))
    dxutAddButton(d, 1, 'OK', 10, 10, 80, 25)
    dxutAddCheckbox(d, 2, 'Check', 10, 40, 120, 20)
    dxutAddEditbox(d, 3, 'abc', 10, 70, 150, 22)
    dxutAddStatic(d, 4, 'Static', 100, 10, 100, 25)
    dxutAddSlider(d, 5, 10, 100, 150, 20, 10)
    dxutAddListbox(d, 6, 10, 130, 200, 60)
    check('dxutGetControlText', get('dxutGetControlText', d, 3) == 'abc')
    dxutSetControlText(d, 1, 'Go')
    check('dxutSetControlText', get('dxutGetControlText', d, 1) == 'Go')
    dxutCheckboxSetChecked(d, 2, true)
    check('dxutCheckboxSetChecked / IsCheckboxChecked', get('dxutIsCheckboxChecked', d, 2) == true)
    dxutSetSliderValue(d, 5, 7)
    check('dxutSetSliderValue / GetSliderValue', get('dxutGetSliderValue', d, 5) == 7)
    dxutListboxInsertItem(d, 6, 'one', 11, -1)
    dxutListboxInsertItem(d, 6, 'two', 22, -1)
    local sel, count = get('dxutGetListboxSelectedItemAndCount', d, 6)
    check('dxutListboxInsertItem / count', count == 2, tostring(sel) .. ' ' .. tostring(count))
    local text, data = get('dxutGetListboxItemTextAndData', d, 6, 1)
    check('dxutGetListboxItemTextAndData', text == 'two' and data == 22, tostring(text) .. ' ' .. tostring(data))
    dxutListboxDeleteItem(d, 6, 0)
    sel, count = get('dxutGetListboxSelectedItemAndCount', d, 6)
    check('dxutListboxDeleteItem', count == 1)
    dxutSetControlVisible(d, 4, false)
    check('dxutSetControlVisible / ControlIsVisible', get('dxutControlIsVisible', d, 4) == false)
    dxutSetControlSize(d, 4, 120, 30)
    dxutSetControlPos(d, 4, 105, 12)
    local cw, ch = get('dxutGetControlSize', d, 4)
    local cx, cy = get('dxutGetControlPos', d, 4)
    check('dxutSet/GetControlSize and Pos', cw == 120 and ch == 30 and cx == 105 and cy == 12)
    dxutEnableDialogCaption(d, false)
    check('dxutEnableDialogCaption / IsDialogCaptionEnabled', get('dxutIsDialogCaptionEnabled', d) == false)
    dxutSetDialogMinimized(d, true)
    check('dxutSetDialogMinimized / IsDialogMinimized', get('dxutIsDialogMinimized', d) == true)
    dxutSetDialogBackgroundColor(d, 0xC0202020)
    dxutSetCheckboxColor(d, 2, 0xFFFF0000)
    dxutSetFocusOnControl(d, 3)
    check('dxutPopEvent: none', get('dxutPopEvent', d) == false)
    dxutDeleteControl(d, 4)
    check('dxutDeleteControl', get('dxutGetControlText', d, 4) == '')
    dxutDeleteDialog(d)
    check('dxutDeleteDialog', get('dxutIsDialogExists', d) == false)
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
    for _, section in ipairs{ state, globals, bitstreams, players, vehicles, textdraws, more, dxut } do
        local ok, err = pcall(section)
        if not ok then out[#out + 1] = 'ERROR section: ' .. tostring(err) end
    end
    write()
end
