--[[
    CLEO+ check: what the Wine tests cannot run (tasks, streaming, weapon
    infos, the camera, line of sight, object creation), through
    moonloader/lib/cleoplus.lua.

    Copy into moonloader/tests/ and run it from the /sh menu: once you are
    spawned it writes moonloader/logs/cleoplus_check.log and unloads. It
    makes one object 3 m north of you and deletes it, sets the rain and
    next weather back to what they were, and locks your movement and the
    camera for a moment. Get in a car first to check
    the car opcodes too.
]]
script_name('cleoplus_check')

local out, passed, failed = {}, 0, 0

local function fmt(v)
    if type(v) == 'number' and math.floor(v) ~= v then return string.format('%.3f', v) end
    return tostring(v)
end

local function check(label, ok, ...)
    local detail = {}
    for i = 1, select('#', ...) do detail[#detail + 1] = fmt(select(i, ...)) end
    out[#out + 1] = string.format('CHECK %-56s %s  %s', label, ok and 'ok' or 'FAIL', table.concat(detail, ', '))
    if ok then passed = passed + 1 else failed = failed + 1 end
end

local function value(label, ...)
    local parts = {}
    for i = 1, select('#', ...) do parts[#parts + 1] = fmt(select(i, ...)) end
    out[#out + 1] = string.format('VALUE %-56s %s', label, table.concat(parts, ', '))
end

local function close(a, b, eps) return type(a) == 'number' and math.abs(a - b) < (eps or 0.01) end

local function run(cp)
    local ped = PLAYER_PED
    local px, py, pz = getCharCoordinates(ped)

    -- the player
    local pct = cp.getCharHealthPercent(ped)
    check('getCharHealthPercent in 0..100+', pct > 0 and pct <= 200, pct)
    value('isCharScriptControlled(player) (the script made it)', cp.isCharScriptControlled(ped))
    value('getCharPedstate / getCharMoveState', cp.getCharPedstate(ped), cp.getCharMoveState(ped))
    check('getTimeCharIsDead alive: -1', cp.getTimeCharIsDead(ped) == -1, cp.getTimeCharIsDead(ped))
    value('getCharStatId / getCharFear', cp.getCharStatId(ped), cp.getCharFear(ped))
    value('getCharProofs', cp.getCharProofs(ped))
    check('isCharOnFire is false', not cp.isCharOnFire(ped))
    check('isCharReallyInAir on the ground (on foot or in a car): false', not cp.isCharReallyInAir(ped))
    value('getCharCollisionSurface / Lighting', cp.getCharCollisionSurface(ped), cp.getCharCollisionLighting(ped))
    check('isCharWeaponVisibleSet', cp.isCharWeaponVisibleSet(ped))
    value('getCharRandomSeed', cp.getCharRandomSeed(ped))

    -- tasks
    local ok, taskId, task = cp.getCharSimplestActiveTask(ped)
    check('getCharSimplestActiveTask has one', ok and taskId >= 0 and task ~= 0, ok, taskId, task)
    if ok then
        check('isCharDoingTaskId of that task', cp.isCharDoingTaskId(ped, taskId), taskId)
        local found, ptr = cp.getCharTaskPointerById(ped, taskId)
        check('getCharTaskPointerById finds the same task', found and ptr == task, found, ptr)
    end
    check('isCharDoingTaskId 9999 is false', not cp.isCharDoingTaskId(ped, 9999))

    -- weapons
    local wok, wi = cp.getWeaponinfo(31, 1)
    check('getWeaponinfo M4 (31)', wok and wi ~= 0, wok, wi)
    if wok then
        local m1 = cp.getWeaponinfoModels(wi)
        check('M4 model is 356', m1 == 356, m1)
        check('M4 slot is 5', cp.getWeaponinfoSlot(wi) == 5, cp.getWeaponinfoSlot(wi))
        check('M4 clip is 50', cp.getWeaponinfoTotalClip(wi) == 50, cp.getWeaponinfoTotalClip(wi))
        check('M4 fire type is instant hit (1)', cp.getWeaponinfoFireType(wi) == 1, cp.getWeaponinfoFireType(wi))
        check('isWeaponFireType(31, 1)', cp.isWeaponFireType(31, 1))
    end
    local cok, cwi = cp.getCurrentCharWeaponinfo(ped)
    check('getCurrentCharWeaponinfo', cok and cwi ~= 0, cok, cwi)
    value('getCharWeaponState / Clip', cp.getCharWeaponState(ped), cp.getCharWeaponClip(ped))

    -- models
    local mok, id = cp.getModelByName('infernus')
    check('getModelByName infernus is 411', mok and id == 411, mok, id)
    check('getModelType(411) is a vehicle (6)', cp.getModelType(411) == 6, cp.getModelType(411))
    local iok, mi = cp.getModelInfo(411)
    check('getModelInfo(411)', iok and mi ~= 0, iok, mi)
    local tok, ptype, pstat = cp.getModelPedTypeAndStat(getCharModel(ped))
    value('getModelPedTypeAndStat of the player model', tok, ptype, pstat)
    value('getModelDoesntExistInRange(1, 20000)', cp.getModelDoesntExistInRange(1, 20000))
    check('isModelAvailableByName("infernus")', cp.isModelAvailableByName('infernus') ~= nil)

    -- the world
    local rain, nextw = cp.getRainIntensity(), cp.getNextWeather()
    cp.setRainIntensity(0.25)
    check('setRainIntensity / getRainIntensity', close(cp.getRainIntensity(), 0.25), cp.getRainIntensity())
    cp.setRainIntensity(rain)
    cp.setNextWeather(nextw)
    check('getNextWeather back', cp.getNextWeather() == nextw, nextw)
    value('getForcedWeather', cp.getForcedWeather())
    value('getDayNightBalance / getUnderwaterness', cp.getDayNightBalance(), cp.getUnderwaterness())
    value('isOnMission / isOnCutscene / isOnScriptedCutscene', cp.isOnMission(), cp.isOnCutscene(), cp.isOnScriptedCutscene())
    value('isHudVisible / isRadarVisible', cp.isHudVisible(), cp.isRadarVisible())
    value('getFadeAlpha', cp.getFadeAlpha())
    value('getFixedXyAspectRatio(100, 100)', cp.getFixedXyAspectRatio(100, 100))
    value('getMouseSensibility', cp.getMouseSensibility())

    -- the camera
    local cam, ccam = cp.getCameraStruct()
    check('getCameraStruct: TheCamera', cam == 0xB6F028, cam, ccam)
    value('getCurrentCameraMode', cp.getCurrentCameraMode())
    local cx, cy, cz = cp.getOffsetFromCameraInWorldCoords(0, 0, 0)
    local ax, ay, az = getActiveCameraCoordinates()
    check('getOffsetFromCameraInWorldCoords(0,0,0) is the camera', close(cx, ax, 0.5) and close(cy, ay, 0.5), cx, cy, cz)
    value('getActiveCameraRotation', cp.getActiveCameraRotation())
    value('getCameraRotationInputValues', cp.getCameraRotationInputValues())
    value('getDistanceMultiplier', cp.getDistanceMultiplier())
    value('getClosestWaterDistance', cp.getClosestWaterDistance())
    value('getThirdPersonCameraTarget(10, player)', cp.getThirdPersonCameraTarget(10, px, py, pz))
    value('getTimeNotTouchingPad(0)', cp.getTimeNotTouchingPad(0))

    -- an object
    local obj = cp.createObjectNoSave(1337, px, py + 3, pz, true, true)
    check('createObjectNoSave 1337', obj and obj ~= -1 and doesObjectExist(obj), obj)
    if obj and obj ~= -1 and doesObjectExist(obj) then
        check('isObjectScriptControlled', cp.isObjectScriptControlled(obj))
        value('getObjectCentreOfMassToBaseOfModel', cp.getObjectCentreOfMassToBaseOfModel(obj))
        value('getObjectProofs', cp.getObjectProofs(obj))
        local _, ox, oy, oz = getObjectCoordinates(obj)
        local ex, ey, ez = cp.getEntityCoordinates(getObjectPointer(obj))
        check('getEntityCoordinates by pointer', close(ex, ox) and close(ey, oy) and close(ez, oz), ex, ey, ez)
        deleteObject(obj)
    end

    -- cops and seeded random numbers
    value('getClosestCopNearPos(player, 200)', cp.getClosestCopNearPos(px, py, pz, 200, true, true, true))
    local a = cp.generateRandomIntInRangeWithSeed(1, 0, 100)
    check('generateRandomIntInRangeWithSeed(1, 0, 100) is 41', a == 41, a)

    -- phase 2: AI flags, locks, names, save slot, damage, easing and noise
    check('isCharUsingGun standing still: false', not cp.isCharUsingGun(ped))
    check('isCharEnteringAnyCar: false', not cp.isCharEnteringAnyCar(ped))
    value('isCharDoingAnyImportantTask(player, 3)', cp.isCharDoingAnyImportantTask(ped, 3))
    value('getCharKillTargetChar(player)', cp.getCharKillTargetChar(ped))
    local namep = cp.getModelNamePointer(411)
    local name = namep ~= 0 and require('ffi').string(require('ffi').cast('const char *', namep)) or ''
    check('getModelNamePointer(411) is "infernus"', name:lower() == 'infernus', name)
    value('getCurrentSaveSlot / isGameFirstStart', cp.getCurrentSaveSlot(), cp.isGameFirstStart())
    local memory = require 'memory'
    local read0 = memory.getuint32(0x510C28)     -- another mod may point it elsewhere already
    cp.setCameraControl(false)
    local zeroed = memory.getuint32(0x510C28) ~= read0 and memory.getfloat(memory.getuint32(0x510C28)) == 0
    cp.setCameraControl(true)
    check('setCameraControl false / true patches and restores', zeroed and memory.getuint32(0x510C28) == read0,
        read0, memory.getuint32(0x510C28))
    cp.setPlayerControlPadMovement(0, false)
    wait(100)
    local locked = bit.band(memory.getuint16(0xB73458 + 0x10E), 1) ~= 0
    cp.setPlayerControlPadMovement(0, true)
    wait(100)
    check('setPlayerControlPadMovement locks and unlocks', locked and bit.band(memory.getuint16(0xB73458 + 0x10E), 1) == 0)
    value('getCharDamageLastFrame(player)', cp.getCharDamageLastFrame(ped))
    check('ease(0.5, 1, 0) is 0.25', math.abs(cp.ease(0.5, 1, 0) - 0.25) < 1e-6, cp.ease(0.5, 1, 0))
    check('perlinNoise(0.5) as CLEO+', math.abs(cp.perlinNoise(0.5) - 0.437432) < 1e-4, cp.perlinNoise(0.5))

    -- a car
    if isCharInAnyCar(ped) then
        local car = storeCarCharIsInNoSave(ped)
        value('getCarAlarm', cp.getCarAlarm(car))
        value('getCarPedals', cp.getCarPedals(car))
        value('getCarValue / getCarAnimgroup', cp.getCarValue(car), cp.getCarAnimgroup(car))
        value('isCarConvertible', cp.isCarConvertible(car))
        value('getCarProofs', cp.getCarProofs(car))
        value('getCarCollisionSurface / Lighting', cp.getCarCollisionSurface(car), cp.getCarCollisionLighting(car))
        check('isCarReallyInAir on the ground: false', not cp.isCarReallyInAir(car))
        value('getTrailerFromCar', cp.getTrailerFromCar(car))
        value('doesCarHavePartNode(car, 1)', cp.doesCarHavePartNode(car, 1))
    else
        out[#out + 1] = 'SKIP  not in a car: the car opcodes were not checked'
    end
end

function main()
    while not isPlayerPlaying(PLAYER_HANDLE) do wait(100) end
    wait(3000)
    local ok, cp = pcall(require, 'cleoplus')
    if not ok then
        out[#out + 1] = 'FAIL  require cleoplus: ' .. tostring(cp)
        failed = failed + 1
    elseif not cp.available then
        out[#out + 1] = 'FAIL  cleoplus: moonsamp.asi has no moonsamp_run'
        failed = failed + 1
    else
        local good, err = pcall(run, cp)
        if not good then
            out[#out + 1] = 'FAIL  error: ' .. tostring(err)
            failed = failed + 1
        end
    end
    table.insert(out, 1, string.format('cleoplus check: %d passed, %d failed', passed, failed))
    local f = io.open(getWorkingDirectory() .. '\\logs\\cleoplus_check.log', 'w')
    if f then
        f:write(table.concat(out, '\n'), '\n')
        f:close()
    end
    sampAddChatMessage(string.format('[cleoplus check] %d passed, %d failed: logs\\cleoplus_check.log',
        passed, failed), 0xFFFFFF)
    thisScript():unload()
end
