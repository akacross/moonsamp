--[[
    NewOpcodes check: what the Wine tests cannot run (RenderWare math, bones,
    car components, line of sight), through moonloader/lib/newopcodes.lua.

    Copy into moonloader/tests/ and run it from the /sh menu: once you are
    spawned it writes moonloader/logs/newopcodes_check.log and unloads. Get in a car first to
    check the component opcodes too. It changes nothing but the scratch
    matrices, quats and colpoint, and sets the ped's and car's alpha to 255.
]]
script_name('newopcodes_check')

local memory = require 'memory'
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

local function run(no)
    -- RenderWare math
    no.initialiseMatrix(1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0)
    no.rotateMatrixOnAxis(1, 0, 0, 1, 90, 0)
    local zangle = no.getMatrixZAngle(1)
    check('rotateMatrixOnAxis 90 about z, then z angle', close(zangle, 0) or close(zangle, 360), zangle)
    local x, y, z = no.getOffsetFromMatrixInWorldCoords(1, 1, 0, 0)
    check('getOffsetFromMatrixInWorldCoords turns x into y', close(x, 0) and close(y, 1) and close(z, 0), x, y, z)
    x, y, z = no.getCoordsOffsetsRelativeToMatrix(0, 1, 0, 1)
    check('getCoordsOffsetsRelativeToMatrix back', close(x, 1) and close(y, 0), x, y, z)
    no.initialiseMatrix(2, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 0)
    no.multiplyMatrices(1, 2, 3)
    x, y, z = no.getMatrixPosition(3)
    check('multiplyMatrices keeps the second position', close(x, 1) and close(y, 2) and close(z, 3), x, y, z)
    no.initialiseQuat(1, 0, 0, 0, 1)
    no.rotateQuatOnAxis(1, 0, 0, 1, 90, 0)
    local qx, qy, qz, qw = no.getQuatElements(1)
    check('rotateQuatOnAxis 90 about z', close(math.abs(qz), 0.7071) and close(math.abs(qw), 0.7071), qx, qy, qz, qw)
    no.interpolateMatrix(4, 2, 1, 0.5)
    zangle = no.getMatrixZAngle(4)
    check('interpolateMatrix halfway: 45 degrees', close(zangle, 315, 0.5) or close(zangle, 225, 0.5), zangle)

    -- the player
    local ped = PLAYER_PED
    local px, py, pz = getCharCoordinates(ped)
    local ok, m = no.getCharBoneMatrix(ped, 8)
    x, y, z = no.getMatrixPosition(m)
    check('getCharBoneMatrix head (8) near the ped, above it', ok and close(x, px, 2) and close(y, py, 2) and z > pz,
        ok, x, y, z)
    local bok, bone = no.getCharBone(ped, 0)
    check('getCharBone frame 0', bok and bone ~= 0, bok, bone)
    value('getCharBone "Head"', no.getCharBone(ped, 'Head'))
    if bok then value('getBoneOffset / getBoneQuat of frame 0', no.getBoneOffset(bone), no.getBoneQuat(bone)) end
    value('getCharMaxHealth', no.getCharMaxHealth(ped))
    local etype = no.getEntityTypeAndClass(getCharPointer(ped))
    check('getEntityTypeAndClass of the ped is 3', etype == 3, etype)
    check('setCharModelAlpha 255', no.setCharModelAlpha(ped, 255))
    value('getCharNumCollidedEntities', no.getCharNumCollidedEntities(ped))
    local tok, txd = no.getModelTxdId(getCharModel(ped))
    check('getModelTxdId of the ped model', tok and txd >= 0, tok, txd)
    value('getModelCrc of the ped model', no.getModelCrc(getCharModel(ped)))

    local hit, hx, hy, hz, entity = no.getCollisionBetweenPoints(px, py, pz + 2, px, py, pz - 5,
        true, false, false, true, false, false, false, false, getCharPointer(ped), 1)
    check('getCollisionBetweenPoints hits the ground below', hit and hz < pz, hit, hx, hy, hz, entity)
    local nx, ny, nz = no.getColDataNormalVector(1)
    check('getColDataNormalVector points up', hit and nz > 0.5, nx, ny, nz)
    value('getColDataSurface / Lighting / Depth', no.getColDataSurface(1), no.getColDataLighting(1), no.getColDataDepth(1))

    -- game state
    local sw, sh = getScreenResolution()
    local w, h = no.getScreenWidthAndHeight()
    check('getScreenWidthAndHeight = getScreenResolution', w == sw and h == sh, w, h, sw, sh)
    check('getCurrentWeather = CWeather::OldWeatherType', no.getCurrentWeather() == memory.getint16(0xC81320),
        no.getCurrentWeather())
    value('getGameVolume', no.getGameVolume())
    value('getSunColors', no.getSunColors())
    value('getSunSize', no.getSunSize())
    value('getSunWorldCoors', no.getSunWorldCoors())
    value('getTrafficlightsCurrentColor', no.getTrafficlightsCurrentColor())
    value('getTxdId "fonts"', no.getTxdId('fonts'))
    value('getLocalTime', no.getLocalTime())
    check('stringLen', no.stringLen('moonsamp') == 8)

    -- a car
    if isCharInAnyCar(ped) then
        local car = storeCarCharIsInNoSave(ped)
        local cx, cy, cz = getCarCoordinates(car)
        local cok, comp = no.getCarComponent(car, 'chassis')
        check('getCarComponent chassis', cok and comp ~= 0, cok, comp)
        if cok then
            check('getComponentName', no.getComponentName(comp) == 'chassis', no.getComponentName(comp))
            x, y, z = no.getMatrixPosition(no.getComponentWorldMatrix(comp))
            check('getComponentWorldMatrix near the car', close(x, cx, 10) and close(y, cy, 10), x, y, z)
            value('getComponentNumObjects / Parent / Child', no.getComponentNumObjects(comp),
                no.getComponentParentComponent(comp), no.getComponentChildComponent(comp))
            local aok, atomic = no.getComponentObject(comp, 0)
            if aok then value('getObjectAtomicNumMaterials', no.getObjectAtomicNumMaterials(atomic)) end
        end
        local mok, mat = no.getCarComponentMatrix(car, 'chassis')
        check('getCarComponentMatrix chassis', mok and mat ~= 0, mok, mat)
        local dok, dx, dy, dz = no.getVehicleDummyPosn(car, 0, true, false)
        check('getVehicleDummyPosn 0 in world coordinates near the car', dok and close(dx, cx, 10), dok, dx, dy, dz)
        value('getVehicleClassAndSubclass', no.getVehicleClassAndSubclass(car))
        check('setCarModelAlpha 255', no.setCarModelAlpha(car, 255))
        value('getCarLightDamageStatus 0', no.getCarLightDamageStatus(car, 0))
        value('getCarNumCollidedEntities', no.getCarNumCollidedEntities(car))
    else
        out[#out + 1] = 'SKIP  not in a car: the component opcodes were not checked'
    end
end

function main()
    while not isPlayerPlaying(PLAYER_HANDLE) do wait(100) end
    wait(3000)
    local ok, no = pcall(require, 'newopcodes')
    if not ok then
        out[#out + 1] = 'FAIL  require newopcodes: ' .. tostring(no)
        failed = failed + 1
    elseif not no.available then
        out[#out + 1] = 'FAIL  newopcodes: moonsamp.asi has no moonsamp_run'
        failed = failed + 1
    else
        local good, err = pcall(run, no)
        if not good then
            out[#out + 1] = 'FAIL  error: ' .. tostring(err)
            failed = failed + 1
        end
    end
    table.insert(out, 1, string.format('newopcodes check: %d passed, %d failed', passed, failed))
    local f = io.open(getWorkingDirectory() .. '\\logs\\newopcodes_check.log', 'w')
    if f then
        f:write(table.concat(out, '\n'), '\n')
        f:close()
    end
    sampAddChatMessage(string.format('[newopcodes check] %d passed, %d failed: logs\\newopcodes_check.log',
        passed, failed), 0xFFFFFF)
    thisScript():unload()
end
