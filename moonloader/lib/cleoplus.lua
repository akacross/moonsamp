--[[ CLEO+ (JuniorDjjr's CLEO plugin, 0E00-0F17) for MoonLoader scripts, run
by moonsamp.asi with or without CLEO:

    local cp = require 'cleoplus'
    if cp.isCharReallyInAir(PLAYER_PED) then print(cp.getCharHealthPercent(PLAYER_PED)) end

A function per opcode, named after it (IS_CHAR_REALLY_IN_AIR ->
isCharReallyInAir). Opcodes with a condition return it first, then their
outputs, as MoonLoader's own functions do. Handles are script handles;
pointers are numbers. Quats and matrices take NewOpcodes' scratch ones (1 to
10) or a pointer.

Only what MoonLoader, Lua, newopcodes.lua and cleo5.lua do not already have
is here; docs/cleoplus.md names what to use for the rest. ]]
local core = require 'moonsamp'

local M = { available = core.available }

-- For tests: see moonsamp.lua.
function M._bind(fn)
    core._bind(fn)
    M.available = core.available
end

local function def(name, op, inputs, outputs, cond)
    core.define(M, name, op, inputs, outputs, cond)
end

-- name, opcode, inputs, outputs, has a condition

-- cars
def('getCarAlarm',                           0x0E00, 'i', 'i')             -- (car): status
def('setCarAlarm',                           0x0E61, 'ii', '')             -- (car, status)
def('isCarScriptControlled',                 0x0E08, 'i', '', true)        -- (car)
def('markCarAsNeeded',                       0x0E09, 'i', '')              -- (car)
def('getTrailerFromCar',                     0x0E59, 'i', 'i', true)       -- (car): trailer
def('getCarFromTrailer',                     0x0E5A, 'i', 'i', true)       -- (car): tractor
def('carHorn',                               0x0E5F, 'i', '')              -- (car)
def('getCarCollisionIntensity',              0x0E65, 'i', 'f', true)       -- (car): intensity
def('getCarCollisionCoordinates',            0x0E66, 'i', 'fff')           -- (car): x, y, z
def('doesCarHavePartNode',                   0x0E82, 'ii', '', true)       -- (car, carNode)
def('getCarCollisionSurface',                0x0E90, 'i', 'i')             -- (car): surfaceType
def('getCarCollisionLighting',               0x0E91, 'i', 'f')             -- (car): lighting
def('isCarReallyInAir',                      0x0E93, 'i', '', true)        -- (car)
def('getCarProofs',                          0x0EAD, 'i', 'bbbbb')         -- (car): bullet, fire, explosion, collision, melee
def('setCarCoordinatesSimple',               0x0EB4, 'ifff', '')           -- (car, x, y, z)
def('getCarRandomSeed',                      0x0EC9, 'i', 'i')             -- (car): randomSeed
def('dontDeleteCarUntilTime',                0x0ECD, 'ii', '')             -- (car, msFromNow)
def('getTimeCarIsDead',                      0x0ECF, 'i', 'i')             -- (car): timeIsDead
def('isCarOwnedByPlayer',                    0x0EF5, 'i', '', true)        -- (car)
def('setCarOwnedByPlayer',                   0x0EF6, 'ib', '')             -- (car, ownedByPlayer)
def('getCarAnimgroup',                       0x0EF9, 'i', 'i')             -- (car): carAnimGroup
def('isCarConvertible',                      0x0EFB, 'i', '', true)        -- (car)
def('getCarValue',                           0x0EFC, 'i', 'i')             -- (car): value
def('getCarPedals',                          0x0EFD, 'i', 'ff')            -- (car): gas, brake
def('getCarWeaponDamageLastFrame',           0x0EB6, 'i', 'iif', true)     -- (car): char, weaponType, intensity

-- chars
def('isCharScriptControlled',                0x0E0A, 'i', '', true)        -- (char)
def('markCharAsNeeded',                      0x0E0B, 'i', '')              -- (char)
def('fixCharGroundBrightnessAndFadeIn',      0x0E24, 'ibbb', '')           -- (char, fixGround, fixBrightness, fadeIn)
def('setCharCoordinatesSimple',              0x0E32, 'ifff', '')           -- (char, x, y, z)
def('getCharHealthPercent',                  0x0E5C, 'i', 'f')             -- (char): healthPercent
def('getCurrentCharWeaponinfo',              0x0E83, 'i', 'u', true)       -- (char): handle
def('getCharWeaponState',                    0x0E8B, 'i', 'i')             -- (char): weaponState
def('getCharWeaponClip',                     0x0E8C, 'i', 'i')             -- (char): weaponClip
def('getCharCollisionSurface',               0x0E8E, 'i', 'i')             -- (char): surfaceType
def('getCharCollisionLighting',              0x0E8F, 'i', 'f')             -- (char): lighting
def('isCharReallyInAir',                     0x0E92, 'i', '', true)        -- (char)
def('clearCharPrimaryTasks',                 0x0E96, 'i', '')              -- (char)
def('clearCharSecondaryTasks',               0x0E97, 'i', '')              -- (char)
def('isCharOnFire',                          0x0EA4, 'i', '', true)        -- (char)
def('getClosestCopNearChar',                 0x0EA5, 'ifbbbb', 'i', true)  -- (char, radius, alive, inCar, onFoot, seenInFront): closestCop
def('getClosestCopNearPos',                  0x0EA6, 'ffffbbb', 'i', true) -- (x, y, z, radius, alive, inCar, onFoot): closestCop
def('setCharArrested',                       0x0EAA, 'i', '')              -- (char)
def('getCharPedstate',                       0x0EAB, 'i', 'i')             -- (char): pedState
def('getCharProofs',                         0x0EAC, 'i', 'bbbbb')         -- (char): bullet, fire, explosion, collision, melee
def('isCharWeaponVisibleSet',                0x0EAF, 'i', '', true)        -- (char)
def('getCharStatId',                         0x0EB1, 'i', 'i')             -- (char): pedStat
def('getCharRandomSeed',                     0x0EC8, 'i', 'i')             -- (char): randomSeed
def('getCharMoveState',                      0x0ECB, 'i', 'i')             -- (char): moveState
def('dontDeleteCharUntilTime',               0x0ECC, 'ii', '')             -- (char, msFromNow)
def('getTimeCharIsDead',                     0x0ECE, 'i', 'i')             -- (char): timeIsDead
def('getCharFear',                           0x0EFA, 'i', 'i')             -- (char): fear
def('isCharDoingTaskId',                     0x0E42, 'ii', '', true)       -- (char, taskId)
def('getCharTaskPointerById',                0x0E43, 'ii', 'u', true)      -- (char, taskId): address
def('getCharSimplestActiveTask',             0x0EFF, 'i', 'iu', true)      -- (char): taskId, address
def('getCharKillTargetChar',                 0x0E44, 'i', 'i', true)       -- (char): killTarget
def('isCharUsingGun',                        0x0E46, 'i', '', true)        -- (char)
def('isCharFighting',                        0x0E47, 'i', '', true)        -- (char)
def('isCharFallenOnGround',                  0x0E48, 'i', '', true)        -- (char)
def('isCharEnteringAnyCar',                  0x0E49, 'i', '', true)        -- (char)
def('isCharExitingAnyCar',                   0x0E4A, 'i', '', true)        -- (char)
def('isCharPlayingAnyScriptAnimation',       0x0E4B, 'ib', '', true)       -- (char, includeAnims)
def('isCharDoingAnyImportantTask',           0x0E4C, 'ib', '', true)       -- (char, includeAnims)
def('getCharDamageLastFrame',                0x0EB5, 'i', 'uiif', true)    -- (char): entity, weaponType, bodyPart, intensity
def('setCharIgnoreDamageAnims',              0x0ED9, 'ib', '')             -- (char, state)

-- objects and car generators
def('isObjectScriptControlled',              0x0E0C, 'i', '', true)        -- (object)
def('markObjectAsNeeded',                    0x0E0D, 'i', '')              -- (object)
def('createObjectNoSave',                    0x0E01, 'ifffbb', 'i')        -- (modelId, x, y, z, useOffset, useGround): handle
def('getObjectCentreOfMassToBaseOfModel',    0x0E71, 'i', 'f')             -- (object): distance
def('isObjectReallyInAir',                   0x0E94, 'i', '', true)        -- (object)
def('simulateObjectDamage',                  0x0E95, 'ifi', '')            -- (object, damage, weaponType)
def('getObjectProofs',                       0x0EAE, 'i', 'bbbbb')         -- (object): bullet, fire, explosion, collision, melee
def('getObjectRandomSeed',                   0x0ECA, 'i', 'i')             -- (object): randomSeed
def('setCarGeneratorNoSave',                 0x0E02, 'i', '')              -- (carGenerator)

-- the world and the game
def('getNextWeather',                        0x0E04, '', 'i')              -- (): type
def('setNextWeather',                        0x0E05, 'i', '')              -- (type)
def('getRainIntensity',                      0x0E06, '', 'f')              -- (): intensity
def('setRainIntensity',                      0x0E07, 'f', '')              -- (intensity)
def('getForcedWeather',                      0x0EB0, '', 'i')              -- (): weather
def('getDayNightBalance',                    0x0E6C, '', 'f')              -- (): intensity
def('getUnderwaterness',                     0x0E6D, '', 'f')              -- (): intensity
def('isOnMission',                           0x0E1D, '', '', true)         -- ()
def('setOnMission',                          0x0F16, 'b', '')              -- (status)
def('isOnCutscene',                          0x0E25, '', '', true)         -- ()
def('isOnScriptedCutscene',                  0x0EB7, '', '', true)         -- ()
def('isRadarVisible',                        0x0EB8, '', '', true)         -- ()
def('isHudVisible',                          0x0EB9, '', '', true)         -- ()
def('isCheatActive',                         0x0E5D, 'i', '', true)        -- (cheat)
def('passTime',                              0x0EBB, 'i', '')              -- (minutes)
def('getFadeAlpha',                          0x0EC7, '', 'f')              -- (): alpha
def('getFixedXyAspectRatio',                 0x0E0F, 'ff', 'ff')           -- (x, y): x, y
def('getCurrentSaveSlot',                    0x0E2C, '', 'i')              -- (): slot
def('isGameFirstStart',                      0x0E2D, '', '', true)         -- ()

-- pickups (MoonLoader v.027 has its own: getAllPickups and the rest)
def('getPickupThisCoord',                    0x0E33, 'fffb', 'i', true)    -- (x, y, z, onlyValid): handle
def('getPickupModel',                        0x0E34, 'i', 'i', true)       -- (pickup): modelId
def('getPickupPointer',                      0x0E38, 'i', 'u', true)       -- (pickup): pointer
def('getPickupType',                         0x0E39, 'i', 'i')             -- (pickup): type

-- input
def('isButtonJustPressed',                   0x0E3E, 'ii', '', true)       -- (pad, buttonId)
def('isAimButtonPressed',                    0x0E67, 'i', '', true)        -- (pad)
def('isAnyFireButtonPressed',                0x0E8D, 'i', '', true)        -- (pad)
def('isSelectMenuJustPressed',               0x0E6E, '', '', true)         -- ()
def('getTimeNotTouchingPad',                 0x0F13, 'i', 'i')             -- (pad): timeInMs
def('getMouseSensibility',                   0x0E23, '', 'f')              -- (): sensibility
def('setCameraControl',                      0x0E60, 'b', '')              -- (state)
def('setPlayerControlPad',                   0x0E68, 'ib', '')             -- (pad, enabled)
def('setPlayerControlPadMovement',           0x0E69, 'ib', '')             -- (pad, movement)

-- weapons
def('isWeaponFireType',                      0x0E26, 'ii', '', true)       -- (weaponType, weaponFire)
def('getWeaponinfo',                         0x0E84, 'ii', 'u', true)      -- (weaponType, weaponSkill): handle
def('getWeaponinfoModels',                   0x0E85, 'i', 'ii')            -- (weaponInfo): model1, model2
def('getWeaponinfoFlags',                    0x0E86, 'i', 'i')             -- (weaponInfo): flags
def('getWeaponinfoAnimgroup',                0x0E87, 'i', 'i')             -- (weaponInfo): animGroup
def('getWeaponinfoTotalClip',                0x0E88, 'i', 'i')             -- (weaponInfo): totalClip
def('getWeaponinfoFireType',                 0x0E89, 'i', 'i')             -- (weaponInfo): fireType
def('getWeaponinfoSlot',                     0x0E8A, 'i', 'i')             -- (weaponInfo): weaponSlot

-- models
def('getModelType',                          0x0E7F, 'i', 'i')             -- (model): type
def('getModelInfo',                          0x0EF8, 'i', 'u', true)       -- (modelId): modelInfo
def('getModelPedTypeAndStat',                0x0EBA, 'i', 'ii', true)      -- (modelId): pedType, pedStat
def('requestPriorityModel',                  0x0E98, 'i', '')              -- (modelId)
def('getModelByName',                        0x0E9C, 's', 'i', true)       -- (name): modelId
def('isModelAvailableByName',                0x0E9D, 's', '', true)        -- (name)
def('getModelDoesntExistInRange',            0x0E9E, 'ii', 'i')            -- (start, end): modelId
def('removeAllUnusedModels',                 0x0E9F, '', '')               -- ()
def('removeModelIfUnused',                   0x0EA3, 'i', '')              -- (modelId)
def('getModelNamePointer',                   0x0F17, 'i', 'u')             -- (modelId): address

-- the camera
def('getCurrentCameraMode',                  0x0E64, '', 'i')              -- (): mode
def('getOffsetFromCameraInWorldCoords',      0x0EB2, 'fff', 'fff')         -- (offsetX, offsetY, offsetZ): x, y, z
def('getThirdPersonCameraTarget',            0x0F0E, 'ffff', 'ffffff')     -- (range, sourceX, sourceY, sourceZ): startX, startY, startZ, endX, endY, endZ
def('getDistanceMultiplier',                 0x0F0F, '', 'ff')             -- (): drawing, generating
def('getActiveCameraRotation',               0x0F10, '', 'fff')            -- (): x, y, z
def('getClosestWaterDistance',               0x0F11, '', 'ff')             -- (): distance, closestZ
def('getCameraStruct',                       0x0F12, '', 'uu')             -- (): cCamera, activeCCam
def('getCameraRotationInputValues',          0x0F14, '', 'ff')             -- (): x, y
def('setCameraRotationInputValues',          0x0F15, 'ff', '')             -- (x, y)

-- entities by pointer, matrices, quats, random numbers
def('getColpointCoordinates',                0x0EE1, 'i', 'fff')           -- (colPoint): x, y, z
def('getEntityCoordinates',                  0x0EEE, 'i', 'fff')           -- (address): x, y, z
def('getEntityHeading',                      0x0EEF, 'i', 'f')             -- (address): heading
def('convertDirectionToQuat',                0x0EB3, 'ifff', '')           -- (quat, x, y, z)
def('setMatrixLookDirection',                0x0F0D, 'iffffff', '')        -- (matrix, originX, originY, originZ, dirX, dirY, dirZ)
def('generateRandomIntInRangeWithSeed',      0x0EBC, 'iii', 'i')           -- (seed, min, max): randomInteger
def('generateRandomFloatInRangeWithSeed',    0x0EBD, 'iff', 'f')           -- (seed, min, max): randomFloat

-- easing and noise: plain math, so computed here, not by an opcode

local pi, sin, cos, sqrt, floor = math.pi, math.sin, math.cos, math.sqrt, math.floor
local band = bit.band

local function bounce_out(k)
    if k < 1 / 2.75 then return 7.5625 * k * k
    elseif k < 2 / 2.75 then k = k - 1.5 / 2.75; return 7.5625 * k * k + 0.75
    elseif k < 2.5 / 2.75 then k = k - 2.25 / 2.75; return 7.5625 * k * k + 0.9375 end
    k = k - 2.625 / 2.75
    return 7.5625 * k * k + 0.984375
end

--[[ Tween.js's easings (MIT), as CLEO+ has them, with its slips fixed: bounce
out returned 0, quintic out overshot and in-out lost its first half, elastic
missed 0 and 1 at the ends. ]]
local ease = {
    [1] = { function(k) return k * k end, function(k) return k * (2 - k) end,
            function(k) k = k * 2; if k < 1 then return 0.5 * k * k end k = k - 1; return -0.5 * (k * (k - 2) - 1) end },
    [2] = { function(k) return k * k * k end, function(k) k = k - 1; return 1 + k * k * k end,
            function(k) k = k * 2; if k < 1 then return 0.5 * k * k * k end k = k - 2; return 0.5 * (k * k * k + 2) end },
    [3] = { function(k) return k * k * k * k end, function(k) k = k - 1; return 1 - k * k * k * k end,
            function(k) k = k * 2; if k < 1 then return 0.5 * k ^ 4 end k = k - 2; return -0.5 * (k ^ 4 - 2) end },
    [4] = { function(k) return k ^ 5 end, function(k) k = k - 1; return 1 + k ^ 5 end,
            function(k) k = k * 2; if k < 1 then return 0.5 * k ^ 5 end k = k - 2; return 0.5 * (k ^ 5 + 2) end },
    [5] = { function(k) return 1 - cos(k * pi / 2) end, function(k) return sin(k * pi / 2) end,
            function(k) return 0.5 * (1 - cos(pi * k)) end },
    [6] = { function(k) return k == 0 and 0 or 1024 ^ (k - 1) end,
            function(k) return k == 1 and 1 or 1 - 2 ^ (-10 * k) end,
            function(k)
                if k == 0 or k == 1 then return k end
                k = k * 2
                if k < 1 then return 0.5 * 1024 ^ (k - 1) end
                return 0.5 * (2 - 2 ^ (-10 * (k - 1)))
            end },
    [7] = { function(k) return 1 - sqrt(1 - k * k) end, function(k) k = k - 1; return sqrt(1 - k * k) end,
            function(k) k = k * 2; if k < 1 then return -0.5 * (sqrt(1 - k * k) - 1) end k = k - 2; return 0.5 * (sqrt(1 - k * k) + 1) end },
    [8] = { function(k) if k == 0 or k == 1 then return k end k = k - 1; return -(2 ^ (10 * k)) * sin((k - 0.1) * 2 * pi / 0.4) end,
            function(k) if k == 0 or k == 1 then return k end return 2 ^ (-10 * k) * sin((k - 0.1) * 2 * pi / 0.4) + 1 end,
            function(k)
                if k == 0 or k == 1 then return k end
                k = k * 2 - 1
                if k < 0 then return -0.5 * 2 ^ (10 * k) * sin((k - 0.1) * 2 * pi / 0.4) end
                return 2 ^ (-10 * k) * sin((k - 0.1) * 2 * pi / 0.4) * 0.5 + 1
            end },
    [9] = { function(k) local s = 1.70158; return k * k * ((s + 1) * k - s) end,
            function(k) local s = 1.70158; k = k - 1; return k * k * ((s + 1) * k + s) + 1 end,
            function(k)
                local s = 2.5949095
                k = k * 2
                if k < 1 then return 0.5 * (k * k * ((s + 1) * k - s)) end
                k = k - 2
                return 0.5 * (k * k * ((s + 1) * k + s) + 2)
            end },
    [10] = { function(k) return 1 - bounce_out(1 - k) end, bounce_out,
             function(k) if k < 0.5 then return (1 - bounce_out(1 - k * 2)) * 0.5 end return bounce_out(k * 2 - 1) * 0.5 + 0.5 end },
}

--[[ Modes 1-10: quadratic, cubic, quartic, quintic, sinusoidal, exponential,
circular, elastic, back, bounce; any other is linear. Ways: 0 in, 1 out, 2
in-out. ]]
-- (k, mode, way): result
function M.ease(k, mode, way)
    local m = ease[mode]
    local f = m and m[(way or 0) + 1]
    if not m then return k end
    return f and f(k) or 0
end

--[[ Sebastien Rombauts' SimplexNoise (MIT), as CLEO+ ships it. ]]
local perm = {
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37,
    240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177,
    33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146,
    158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25,
    63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100,
    109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206,
    59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153,
    101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246,
    97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49,
    192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93,
    222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
}
local function hash(i) return perm[i % 256 + 1] end

local function grad1(h, x)
    h = band(h, 15)
    local g = 1 + band(h, 7)
    if band(h, 8) ~= 0 then g = -g end
    return g * x
end

local function grad2(h, x, y)
    h = band(h, 63)
    local u, v = h < 4 and x or y, h < 4 and y or x
    return (band(h, 1) ~= 0 and -u or u) + (band(h, 2) ~= 0 and -2 * v or 2 * v)
end

local function grad3(h, x, y, z)
    h = band(h, 15)
    local u = h < 8 and x or y
    local v = h < 4 and y or ((h == 12 or h == 14) and x or z)
    return (band(h, 1) ~= 0 and -u or u) + (band(h, 2) ~= 0 and -v or v)
end

local function corner2(t, h, x, y)
    if t < 0 then return 0 end
    t = t * t
    return t * t * grad2(h, x, y)
end

local function corner3(t, h, x, y, z)
    if t < 0 then return 0 end
    t = t * t
    return t * t * grad3(h, x, y, z)
end

local function noise1(x)
    local i0 = floor(x)
    local x0 = x - i0
    local x1 = x0 - 1
    local t0, t1 = 1 - x0 * x0, 1 - x1 * x1
    t0, t1 = t0 * t0, t1 * t1
    return 0.395 * (t0 * t0 * grad1(hash(i0), x0) + t1 * t1 * grad1(hash(i0 + 1), x1))
end

local function noise2(x, y)
    local F2, G2 = 0.366025403, 0.211324865
    local s = (x + y) * F2
    local i, j = floor(x + s), floor(y + s)
    local t = (i + j) * G2
    local x0, y0 = x - (i - t), y - (j - t)
    local i1, j1 = 0, 1
    if x0 > y0 then i1, j1 = 1, 0 end
    local x1, y1 = x0 - i1 + G2, y0 - j1 + G2
    local x2, y2 = x0 - 1 + 2 * G2, y0 - 1 + 2 * G2
    return 45.23065 * (corner2(0.5 - x0 * x0 - y0 * y0, hash(i + hash(j)), x0, y0)
        + corner2(0.5 - x1 * x1 - y1 * y1, hash(i + i1 + hash(j + j1)), x1, y1)
        + corner2(0.5 - x2 * x2 - y2 * y2, hash(i + 1 + hash(j + 1)), x2, y2))
end

local function noise3(x, y, z)
    local G3 = 1 / 6
    local s = (x + y + z) / 3
    local i, j, k = floor(x + s), floor(y + s), floor(z + s)
    local t = (i + j + k) * G3
    local x0, y0, z0 = x - (i - t), y - (j - t), z - (k - t)
    local i1, j1, k1, i2, j2, k2
    if x0 >= y0 then
        if y0 >= z0 then i1, j1, k1, i2, j2, k2 = 1, 0, 0, 1, 1, 0
        elseif x0 >= z0 then i1, j1, k1, i2, j2, k2 = 1, 0, 0, 1, 0, 1
        else i1, j1, k1, i2, j2, k2 = 0, 0, 1, 1, 0, 1 end
    else
        if y0 < z0 then i1, j1, k1, i2, j2, k2 = 0, 0, 1, 0, 1, 1
        elseif x0 < z0 then i1, j1, k1, i2, j2, k2 = 0, 1, 0, 0, 1, 1
        else i1, j1, k1, i2, j2, k2 = 0, 1, 0, 1, 1, 0 end
    end
    local x1, y1, z1 = x0 - i1 + G3, y0 - j1 + G3, z0 - k1 + G3
    local x2, y2, z2 = x0 - i2 + 2 * G3, y0 - j2 + 2 * G3, z0 - k2 + 2 * G3
    local x3, y3, z3 = x0 - 1 + 3 * G3, y0 - 1 + 3 * G3, z0 - 1 + 3 * G3
    return 32 * (corner3(0.6 - x0 * x0 - y0 * y0 - z0 * z0, hash(i + hash(j + hash(k))), x0, y0, z0)
        + corner3(0.6 - x1 * x1 - y1 * y1 - z1 * z1, hash(i + i1 + hash(j + j1 + hash(k + k1))), x1, y1, z1)
        + corner3(0.6 - x2 * x2 - y2 * y2 - z2 * z2, hash(i + i2 + hash(j + j2 + hash(k + k2))), x2, y2, z2)
        + corner3(0.6 - x3 * x3 - y3 * y3 - z3 * z3, hash(i + 1 + hash(j + 1 + hash(k + 1))), x3, y3, z3))
end

local function fractal(noise, octaves, frequency, amplitude, lacunarity, persistence, ...)
    local out, denom, n = 0, 0, select('#', ...)
    for _ = 1, octaves do
        local a, b, c = ...
        if n == 1 then out = out + amplitude * noise(a * frequency)
        elseif n == 2 then out = out + amplitude * noise(a * frequency, b * frequency)
        else out = out + amplitude * noise(a * frequency, b * frequency, c * frequency) end
        denom = denom + amplitude
        frequency = frequency * lacunarity
        amplitude = amplitude * persistence
    end
    return denom ~= 0 and out / denom or 0
end

-- (x): result
function M.perlinNoise(x) return noise1(x) end

-- (x, octaves, frequency, amplitude, lacunarity, persistence): result
function M.perlinNoiseFractal(x, octaves, frequency, amplitude, lacunarity, persistence)
    return fractal(noise1, octaves, frequency, amplitude, lacunarity, persistence, x)
end

-- (x, y, octaves, frequency, amplitude, lacunarity, persistence): result
function M.perlinNoiseFractal2d(x, y, octaves, frequency, amplitude, lacunarity, persistence)
    return fractal(noise2, octaves, frequency, amplitude, lacunarity, persistence, x, y)
end

-- (x, y, z, octaves, frequency, amplitude, lacunarity, persistence): result
function M.perlinNoiseFractal3d(x, y, z, octaves, frequency, amplitude, lacunarity, persistence)
    return fractal(noise3, octaves, frequency, amplitude, lacunarity, persistence, x, y, z)
end

return M
