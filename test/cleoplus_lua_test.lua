-- moonloader/lib/cleoplus.lua under LuaJIT, with a fake moonsamp_run that
-- records each call and hands back set outputs (run.sh runs it).
package.path = arg[0]:gsub('[^/]+$', '') .. '../moonloader/lib/?.lua;' .. package.path
local ffi = require 'ffi'
local cp = require 'cleoplus'

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

check(not cp.available, 'no moonsamp.asi to load here')
cp._bind(fake)
check(cp.available, '_bind makes it available')

give, give_cond = { fbits(25) }, 0
local pct = cp.getCharHealthPercent(3)
check(last.op == 0x0E5C and last.types == 'i' and pct == 25, 'getCharHealthPercent: 0E5C, a handle in, a float out')

give, give_cond = { 0x0400 }, 1
local ok, trailer = cp.getTrailerFromCar(5)
check(ok == true and trailer == 0x0400 and last.op == 0x0E59, 'a condition comes first, then the outputs')

give, give_cond = {}, 0
check(cp.isCharReallyInAir(1) == false and last.op == 0x0E92, 'a bare condition returns a boolean')

cp.setCarAlarm(7, 2)
check(last.op == 0x0E61 and last.types == 'ii' and last.input[2] == 2, 'setCarAlarm: two ints')

cp.setRainIntensity(0.5)
check(last.op == 0x0E07 and last.input[1] == fbits(0.5), 'setRainIntensity: the float goes as its bits')

give = { 1, 0, 1, 0, 1 }
local b, f, e, c, m = cp.getCharProofs(1)
check(b == true and f == false and e == true and c == false and m == true, 'getCharProofs: five booleans')

give, give_cond = { 1000, 0x12345678 }, 1
local ok2, id, task = cp.getCharSimplestActiveTask(1)
check(ok2 and id == 1000 and task == 0x12345678, 'getCharSimplestActiveTask: an id and a pointer')

cp.getModelByName('infernus')
check(last.op == 0x0E9C and last.types == 's', 'getModelByName takes a string')

-- easing and noise run here, not through moonsamp_run
local plain = { ease = true, perlinNoise = true, perlinNoiseFractal = true, perlinNoiseFractal2d = true,
    perlinNoiseFractal3d = true, _bind = true }
local count = 0
for name, f in pairs(cp) do
    if type(f) == 'function' and not plain[name] then count = count + 1 end
end
check(count == 139, ('a function per opcode in src/cleoplus.c (%d)'):format(count))

local ends = true
for mode = 0, 10 do
    for way = 0, 2 do
        if math.abs(cp.ease(0, mode, way)) > 1e-9 or math.abs(cp.ease(1, mode, way) - 1) > 1e-9 then ends = false end
    end
end
check(ends, 'every easing goes from 0 to 1')
check(math.abs(cp.ease(0.3, 4, 1) - 0.83193) < 1e-5, 'quintic out (CLEO+ overshot to 1.168)')
check(cp.ease(0.3, 99, 0) == 0.3, 'an unknown mode is linear')

-- CLEO+'s own SimplexNoise.cpp, built natively, gave these
local ref = {
    { 'n1', -3.7, 0.762325 },
    { 'n1', 0.5, 0.437432 },
    { 'n1', 123.456, 0.813059 },
    { 'n2', -3.7, -0.334579 },
    { 'n2', 0.5, -0.757499 },
    { 'n2', 123.456, -0.671865 },
    { 'n3', -3.7, 0.569561 },
    { 'n3', 0.5, 0.064485 },
    { 'n3', 123.456, -0.025478 },
    { 'f1', -3.7, -0.399909 },
    { 'f1', 0.5, 0.055617 },
    { 'f1', 123.456, 0.250197 },
    { 'f2', -3.7, -0.005972 },
    { 'f2', 0.5, 0.273502 },
    { 'f2', 123.456, 0.070927 },
    { 'f3', -3.7, -0.174349 },
    { 'f3', 0.5, 0.245881 },
    { 'f3', 123.456, 0.194280 },
}
local worst = 0
for _, r in ipairs(ref) do
    local k, x, v = r[1], r[2], r[3]
    local got
    if k == 'n1' then got = cp.perlinNoise(x)
    elseif k == 'n2' then got = cp.perlinNoiseFractal2d(x, x * 0.7 - 1.1, 1, 1, 1, 2, 0.5)
    elseif k == 'n3' then got = cp.perlinNoiseFractal3d(x, x * 0.7 - 1.1, 2.5 - x, 1, 1, 1, 2, 0.5)
    elseif k == 'f1' then got = cp.perlinNoiseFractal(x, 4, 1.5, 2, 2, 0.5)
    elseif k == 'f2' then got = cp.perlinNoiseFractal2d(x, x * 0.3, 3, 1.5, 2, 2, 0.5)
    else got = cp.perlinNoiseFractal3d(x, x * 0.3, -x, 5, 1.5, 2, 2, 0.5) end
    worst = math.max(worst, math.abs(got - v))
end
check(worst < 1e-4, ('noise matches CLEO+ (worst %.1e)'):format(worst))

print(('\n%d passed, %d failed'):format(passed, failed))
os.exit(failed == 0 and 0 or 1)
