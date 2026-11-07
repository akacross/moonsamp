--[[ NewOpcodes (DK22Pac's CLEO plugin, 0D00-0D7F) for MoonLoader scripts,
run by moonsamp.asi:

    local no = require 'newopcodes'
    local ok, matrix = no.getCharBoneMatrix(PLAYER_PED, 5)
    if ok then print(no.getMatrixPosition(matrix)) end

A function per opcode, named after it (GET_CHAR_BONE_MATRIX ->
getCharBoneMatrix). Opcodes with a condition return it first, then their
outputs, as MoonLoader's own functions do. Handles are script handles;
pointers are numbers. As in NewOpcodes, a matrix, quat or vector of 1 to 10
is one of ten scratch ones in moonsamp, and colpoint 1 the scratch colpoint.
Angles are degrees, but setMatrixRotation takes radians (NewOpcodes passes
them to CMatrix::SetRotate as they are). The drawing and CLEO audio stream
opcodes are not there yet. ]]
local core = require 'moonsamp'

local M = { available = core.available }

-- For tests: see moonsamp.lua.
function M._bind(fn)
    core._bind(fn)
    M.available = core.available
end

local function call(op, inputs, outputs, cond, ...)
    return core.call(op, inputs, outputs, cond, ...)
end

local function def(name, op, inputs, outputs, cond)
    core.define(M, name, op, inputs, outputs, cond)
end

-- name, opcode, inputs, outputs, has a condition
def('multiplyMatrices',                  0x0D00, 'iii', '')           -- (a, b, out): out = a * b
def('rotateMatrixOnAxis',                0x0D01, 'iffffi', '')        -- (m, x, y, z, angle, combineOp)
def('getMatrixXAngle',                   0x0D02, 'i', 'f')
def('getMatrixYAngle',                   0x0D03, 'i', 'f')
def('getMatrixZAngle',                   0x0D04, 'i', 'f')
def('setMatrixPosition',                 0x0D05, 'ifff', '')
def('getMatrixPosition',                 0x0D06, 'i', 'fff')
def('getCoordsOffsetsRelativeToMatrix',  0x0D07, 'fffi', 'fff')       -- (x, y, z, m)
def('setMatrixRotation',                 0x0D08, 'ifff', '')          -- radians
def('copyMatrix',                        0x0D09, 'ii', '')            -- (dst, src)
def('getOffsetFromMatrixInWorldCoords',  0x0D0A, 'ifff', 'fff')
def('getCharBoneMatrix',                 0x0D0B, 'ii', 'u', true)
def('getCarComponentMatrix',             0x0D0C, 'is', 'u', true)
def('getCarComponent',                   0x0D0D, 'is', 'u', true)
def('setCarComponentState',              0x0D0E, 'isi', '', true)
def('setCarModelAlpha',                  0x0D0F, 'ii', '', true)
def('setCharModelAlpha',                 0x0D10, 'ii', '', true)
def('setObjectModelAlpha',               0x0D11, 'ii', '', true)
def('setCarComponentModelAlpha',         0x0D12, 'isi', '', true)
def('setMatrixXRotation',                0x0D13, 'if', '')
def('setMatrixYRotation',                0x0D14, 'if', '')
def('setMatrixZRotation',                0x0D15, 'if', '')
def('setMatrixRotationFromQuat',         0x0D16, 'ii', '')            -- (m, q)
def('setQuatFromMatrix',                 0x0D17, 'ii', '')            -- (m, q)
def('rotateQuatOnAxis',                  0x0D18, 'iffffi', '')        -- (q, x, y, z, angle, combineOp)
def('getNormalisedQuat',                 0x0D19, 'ii', '')            -- (src, dst)
def('multiplyQuats',                     0x0D1A, 'iii', '')           -- (a, b, out)
def('getEntityTypeAndClass',             0x0D1B, 'i', 'iu')           -- (pointer): type, vtable
def('normaliseVector',                   0x0D1C, 'i', '')
def('interpolateMatrix',                 0x0D1D, 'iiif', '')          -- (out, from, to, t)
def('quatSlerp',                         0x0D1E, 'iiif', '')          -- (out, from, to, t)
def('getComponentChildComponent',        0x0D1F, 'i', 'u')
def('getComponentNextComponent',         0x0D20, 'i', 'u')
def('getComponentName',                  0x0D21, 'i', 's')
def('getComponentWorldMatrix',           0x0D22, 'i', 'u')
def('getComponentModellingMatrix',       0x0D23, 'i', 'u')
def('initialiseQuat',                    0x0D24, 'iffff', '')         -- (q, x, y, z, w)
def('initialiseMatrix',                  0x0D25, 'ifffifffifffifffi', '') -- right, flags, up, pad, at, pad, pos, pad
def('initialiseVector',                  0x0D26, 'ifff', '')
def('memcpy',                            0x0D27, 'iii', '')           -- (src, dst, size)
def('getVectorElements',                 0x0D28, 'i', 'fff')
def('getQuatElements',                   0x0D29, 'i', 'ffff')
def('getCarNumCollidedEntities',         0x0D2A, 'i', 'i')
def('getCharNumCollidedEntities',        0x0D2B, 'i', 'i')
def('getObjectNumCollidedEntities',      0x0D2C, 'i', 'i')
def('getLocalTime',                      0x0D2D, '', 'iiiiiiii')      -- year, month, day of week, day, h, m, s, ms
def('setThreadVar',                      0x0D2E, 'iii', '')           -- (script pointer, var, value)
def('getThreadVar',                      0x0D2F, 'ii', 'i')
def('getCharBone',                       0x0D30, 'ix', 'u', true)     -- (ped, bone name or frame index)
def('getBoneOffset',                     0x0D31, 'i', 'u')
def('getBoneQuat',                       0x0D32, 'i', 'u')
def('setCarDoorWindowState',             0x0D33, 'iib', '')
def('getCarCollidedEntities',            0x0D34, 'i', 'uuuuuu')
def('getCharCollidedEntities',           0x0D35, 'i', 'uuuuuu')
def('getObjectCollidedEntities',         0x0D36, 'i', 'uuuuuu')
def('setStructParam',                    0x0D37, 'iii', '')           -- (pointer, index, value): 4-byte elements
def('getStructParam',                    0x0D38, 'ii', 'i')
def('getCharMaxHealth',                  0x0D39, 'i', 'f')
-- (from xyz, to xyz, buildings, vehicles, peds, objects, dummies,
-- seeThrough, cameraIgnore, shootThrough, ignoreEntity, colPoint):
-- hit, x, y, z, entity
def('getCollisionBetweenPoints',         0x0D3A, 'ffffffbbbbbbbbii', 'fffu', true)
def('getColDataNormalVector',            0x0D3B, 'i', 'fff')
def('getColDataSurface',                 0x0D3C, 'i', 'i')
def('getColDataLighting',                0x0D3D, 'i', 'i')
def('getColDataDepth',                   0x0D3E, 'i', 'f')
def('findIntersectionBetweenCircles',    0x0D3F, 'ffffff', 'ffff', true)
def('getTxdId',                          0x0D43, 's', 'i')
def('getModelTxdId',                     0x0D47, 'i', 'i', true)
def('getModelCrc',                       0x0D48, 'i', 'u', true)
def('stringCmp',                         0x0D49, 'xx', 'i', true)     -- true when equal
def('stringCat',                         0x0D4A, 'is', '')            -- (buffer pointer, text)
def('stringStr',                         0x0D4B, 'xx', 'u', true)     -- a pointer into the first
def('stringLen',                         0x0D4C, 'x', 'i')
def('stringCpy',                         0x0D4D, 'xi', '')            -- (text, buffer pointer)
def('getStructField',                    0x0D4E, 'iii', 'i')          -- (pointer, offset, size)
def('setStructField',                    0x0D4F, 'iiix', '')          -- (pointer, offset, size, value or text)
def('getSunColors',                      0x0D55, '', 'iiiiii')        -- core rgb, glow rgb
def('getSunScreenCoors',                 0x0D56, '', 'ff')
def('getSunWorldCoors',                  0x0D57, '', 'fff', true)     -- false below the horizon
def('getSunSize',                        0x0D58, '', 'ff')            -- core, glow
def('getCurrentWeather',                 0x0D59, '', 'i')
def('getTrafficlightsCurrentColor',      0x0D5A, '', 'ii')            -- north-south, west-east
def('getCarLightDamageStatus',           0x0D5C, 'ii', 'b')
def('setCarLightDamageStatus',           0x0D5D, 'iib', '')
def('getVehicleClassAndSubclass',        0x0D5E, 'i', 'ii')
def('getVehicleDummyPosn',               0x0D5F, 'iibb', 'fff', true) -- (car, dummy, world, mirrored)
-- (type, from entity, origin xyz, target xyz, target entity, force)
def('createProjectile',                  0x0D60, 'iiffffffif', '', true)
def('getComponentParentComponent',       0x0D74, 'i', 'u')
def('getComponentNumObjects',            0x0D75, 'i', 'i')
def('getComponentObject',                0x0D76, 'ii', 'u', true)
def('hideObjectAtomic',                  0x0D77, 'ib', '')
def('getObjectAtomicFlag',               0x0D78, 'ii', 'b')
def('setObjectAtomicFlag',               0x0D79, 'iib', '')
def('getObjectAtomicNumMaterials',       0x0D7A, 'i', 'i', true)
def('getObjectAtomicMaterialTexture',    0x0D7B, 'ii', 'u', true)

-- (asFloat): sfx and radio volume, or screen width and height
function M.getGameVolume(asFloat)
    return call(0x0D72, 'b', asFloat and 'ff' or 'ii', false, asFloat)
end

function M.getScreenWidthAndHeight(asFloat)
    return call(0x0D73, 'b', asFloat and 'ff' or 'ii', false, asFloat)
end

return M
