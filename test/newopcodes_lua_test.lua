-- moonloader/lib/newopcodes.lua under LuaJIT, with a fake moonsamp_run that
-- records each call and hands back set outputs (run.sh runs it).
package.path = arg[0]:gsub('[^/]+$', '') .. '../moonloader/lib/?.lua;' .. package.path
local ffi = require 'ffi'
local no = require 'newopcodes'

local failed, passed = 0, 0
local function check(ok, what)
    print((ok and 'ok   ' or 'FAIL ') .. what)
    if ok then passed = passed + 1 else failed = failed + 1 end
end

local bits = ffi.new('union { float f; int32_t i; }')
local function fbits(f) bits.f = f; return bits.i end

local last, give, give_cond = {}, {}, 0
local fake = ffi.cast('int (*)(int, const char *, const int32_t *, int, int32_t *)',
    function(op, types, input, nout, out)
        last = { op = op, types = ffi.string(types), nout = nout, input = {} }
        for k = 0, #last.types - 1 do last.input[k + 1] = input[k] end
        for k = 0, nout - 1 do out[k] = give[k + 1] or 0 end
        return give_cond
    end)

check(not no.available, 'no moonsamp.asi to load here')
check(not pcall(no.getCurrentWeather), 'a call without it is an error')
no._bind(fake)
check(no.available, '_bind makes it available')

no.setMatrixPosition(1, 1.5, -2, 3)
check(last.op == 0x0D05 and last.types == 'ifff' and last.nout == 0, 'setMatrixPosition: 0D05, int then three floats')
check(last.input[1] == 1 and last.input[2] == fbits(1.5) and last.input[3] == fbits(-2), 'floats go as their bits')

give = { fbits(10), fbits(20.5), fbits(-30) }
local x, y, z = no.getMatrixPosition(1)
check(x == 10 and y == 20.5 and z == -30, 'getMatrixPosition returns three floats')

give, give_cond = { 0x12345678 }, 1
local ok, m = no.getCharBoneMatrix(2, 5)
check(ok == true and m == 0x12345678 and last.op == 0x0D0B, 'a condition comes first, then the outputs')
give_cond = 0
ok = no.getCharBoneMatrix(2, 5)
check(ok == false, 'false when the opcode says so')

no.getCharBone(2, 'bip01 head')
check(last.types == 'is', 'getCharBone takes a bone name as text')
no.getCharBone(2, 7)
check(last.types == 'ii' and last.input[2] == 7, 'or a frame index as an int')

no.setCarModelAlpha(0xC0000001, 128)
check(last.input[1] == -1073741823, 'a value over 2^31 goes as its int32')
give, give_cond = { -1073741823 }, 1
local _, comp = no.getCarComponent(1, 'wheel_lf_dummy')
check(comp == 0xC0000001, 'and comes back unsigned')

no.setCarDoorWindowState(1, 2, true)
check(last.types == 'iii' and last.input[3] == 1, 'a boolean goes as 1')

give = { fbits(1920), fbits(1080) }
local w, h = no.getScreenWidthAndHeight(true)
check(last.op == 0x0D73 and last.input[1] == 1 and w == 1920 and h == 1080, 'getScreenWidthAndHeight(true) reads floats')
give = { 1280, 720 }
w, h = no.getScreenWidthAndHeight()
check(last.input[1] == 0 and w == 1280 and h == 720, 'and ints without it')

give, give_cond = { 1 }, 1
local same = no.stringCmp('a', 'a')
check(same == true and last.types == 'ss', 'stringCmp sends two strings')

no.getCollisionBetweenPoints(0, 0, 0, 1, 1, 1, true, true, false, false, false, false, false, false, 0, 1)
check(last.types == 'ffffffiiiiiiiiii' and last.nout == 4 and last.input[7] == 1 and last.input[9] == 0,
    'getCollisionBetweenPoints: 16 inputs, 4 outputs')

local names, ops = 0, {}
give = {}
for name, fn in pairs(no) do
    if type(fn) == 'function' and name ~= '_bind' then
        names = names + 1
        fn(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1)
        if last.op < 0x0D00 or last.op > 0x0D7F or ops[last.op] then
            check(false, name .. ' runs a NewOpcodes opcode of its own')
        end
        ops[last.op] = name
    end
end
local dir = arg[0]:gsub('[^/]+$', '') .. '../src/'
local c = io.open(dir .. 'newopcodes.c'):read('*a') .. io.open(dir .. 'draw.c'):read('*a')
local registered, missing = 0, {}
for hex in c:gmatch('{ 0x(0D%x%x), op_') do
    registered = registered + 1
    if not ops[tonumber(hex, 16)] then missing[#missing + 1] = hex end
end
check(registered > 0 and names == registered and #missing == 0,
    ('a function for each of the %d opcodes newopcodes.c and draw.c register (%d, missing %s)')
        :format(registered, names, table.concat(missing, ' ')))

print(('%d passed, %d failed'):format(passed, failed))
os.exit(failed == 0 and 0 or 1)
