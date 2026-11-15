--[[
    MoonLoader v.027 pickup functions check: getAllPickups, getPickupModel,
    getPickupType, getPickupPointer, getPickupPointerHandle, isCursorActive.

    Copy into moonloader/tests/ and run it from the /sh menu: once you are
    spawned it writes moonloader/logs/pickups_check.log and unloads. It makes
    one pickup 4 m north of you and removes it, and shows the cursor briefly.
    Needs MoonLoader v.027; on 026.5 every check fails.
]]
script_name('pickups_check')

local memory = require 'memory'
local out, passed, failed = {}, 0, 0

local function check(label, ok, ...)
    local detail = {}
    for i = 1, select('#', ...) do detail[#detail + 1] = tostring(select(i, ...)) end
    out[#out + 1] = string.format('CHECK %-56s %s  %s', label, ok and 'ok' or 'FAIL', table.concat(detail, ', '))
    if ok then passed = passed + 1 else failed = failed + 1 end
end

local function value(label, ...)
    local parts = {}
    for i = 1, select('#', ...) do parts[#parts + 1] = tostring(select(i, ...)) end
    out[#out + 1] = string.format('VALUE %-56s %s', label, table.concat(parts, ', '))
end

local function listed(handle)
    for _, h in ipairs(getAllPickups()) do
        if h == handle then return true end
    end
    return false
end

local function run()
    for _, name in ipairs({'getAllPickups', 'getPickupModel', 'getPickupType', 'getPickupPointer',
        'getPickupPointerHandle', 'isCursorActive'}) do
        check(name .. ' exists', type(_G[name]) == 'function', type(_G[name]))
    end
    if type(getAllPickups) ~= 'function' then return end

    -- every pickup the game has now
    local all = getAllPickups()
    check('getAllPickups returns a table', type(all) == 'table', type(all))
    value('pickups in use', #all)
    local bad = 0
    for _, h in ipairs(all) do
        local ptr = getPickupPointer(h)
        local model, ptype = getPickupModel(h), getPickupType(h)
        if ptr == 0 or ptype < 1 or ptype > 22 or getPickupPointerHandle(ptr) ~= h
            or memory.getint16(ptr + 0x18) ~= model or memory.getuint8(ptr + 0x1C) ~= ptype then
            bad = bad + 1
            value('odd pickup', string.format('0x%08X', h), model, ptype, string.format('0x%08X', ptr))
        end
    end
    check('every listed pickup reads back the same', bad == 0, bad .. ' odd of ' .. #all)

    -- one of our own
    local x, y, z = getCharCoordinates(PLAYER_PED)
    local h = createPickup(1239, 3, x, y + 4, z)
    value('createPickup 1239 type 3', string.format('0x%08X', h))
    check('our pickup is listed', listed(h))
    check('getPickupModel is 1239', getPickupModel(h) == 1239, getPickupModel(h))
    check('getPickupType is 3', getPickupType(h) == 3, getPickupType(h))
    local ptr = getPickupPointer(h)
    check('getPickupPointer is not 0', ptr ~= 0, string.format('0x%08X', ptr))
    check('getPickupPointerHandle(pointer) gives the handle back', getPickupPointerHandle(ptr) == h,
        getPickupPointerHandle(ptr))
    local px, py = memory.getint16(ptr + 0x10) / 8, memory.getint16(ptr + 0x12) / 8
    check('CPickup position is where we made it', math.abs(px - x) < 0.5 and math.abs(py - (y + 4)) < 0.5,
        string.format('%.2f %.2f', px, py))
    check('CPickup reference count is the high 16 bits',
        memory.getint16(ptr + 0x1A) == bit.arshift(h, 16), memory.getint16(ptr + 0x1A))

    removePickup(h)
    check('removed: not listed', not listed(h))
    check('removed: getPickupModel is 0', getPickupModel(h) == 0, getPickupModel(h))
    check('removed: getPickupType is 0', getPickupType(h) == 0, getPickupType(h))
    check('removed: getPickupPointer is 0', getPickupPointer(h) == 0, getPickupPointer(h))
    check('removed: getPickupPointerHandle(old pointer) is -1', getPickupPointerHandle(ptr) == -1,
        getPickupPointerHandle(ptr))

    -- handles and pointers that are not pickups
    check('getPickupModel(-1) is 0', getPickupModel(-1) == 0, getPickupModel(-1))
    check('getPickupPointerHandle(0) is -1', getPickupPointerHandle(0) == -1, getPickupPointerHandle(0))
    check('getPickupPointerHandle(ped pointer) is -1', getPickupPointerHandle(getCharPointer(PLAYER_PED)) == -1,
        getPickupPointerHandle(getCharPointer(PLAYER_PED)))

    -- the cursor
    local before = isCursorActive()
    showCursor(true, false)
    local shown = isCursorActive()
    showCursor(false)
    local hidden = isCursorActive()
    check('isCursorActive follows showCursor', shown == true and hidden == false, before, shown, hidden)
end

function main()
    while not isPlayerPlaying(PLAYER_HANDLE) do wait(100) end
    wait(3000)
    local good, err = pcall(run)
    if not good then
        out[#out + 1] = 'FAIL  error: ' .. tostring(err)
        failed = failed + 1
    end
    table.insert(out, 1, string.format('pickups check (%s): %d passed, %d failed', getMoonloaderVersion(), passed, failed))
    local f = io.open(getWorkingDirectory() .. '\\logs\\pickups_check.log', 'w')
    if f then
        f:write(table.concat(out, '\n'), '\n')
        f:close()
    end
    sampAddChatMessage(string.format('[pickups check] %d passed, %d failed: logs\\pickups_check.log',
        passed, failed), 0xFFFFFF)
    thisScript():unload()
end
