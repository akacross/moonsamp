-- moonloader/lib/cleo5.lua under LuaJIT, with a fake moonsamp_run that
-- records each call and hands back set outputs (run.sh runs it).
package.path = arg[0]:gsub('[^/]+$', '') .. '../moonloader/lib/?.lua;' .. package.path
local ffi = require 'ffi'
io.stdout:setvbuf('no')
local cleo5 = require 'cleo5'

local failed, passed = 0, 0
local function check(ok, what)
    print((ok and 'ok   ' or 'FAIL ') .. what)
    if ok then passed = passed + 1 else failed = failed + 1 end
end

local bits = ffi.new('union { float f; int32_t i; }')
local function fbits(f) bits.f = f; return bits.i end

-- Lua strings are interned: the text the lib sent is at the address this
-- one has (a 64-bit LuaJIT here only sees the low 32 bits of it).
local core = require 'moonsamp'
local last, give, give_cond = {}, {}, 0
local function sent(k, text)
    return last.input[k] == core.i32(ffi.cast('uintptr_t', ffi.cast('const char *', text)))
end
local fake = ffi.cast('int (*)(int, const char *, const int32_t *, int, int32_t *)',
    function(op, types, input, nout, out)
        last = { op = op, types = ffi.string(types), nout = nout, input = {} }
        for k = 0, #last.types - 1 do last.input[k + 1] = input[k] end
        for k = 0, nout + 8 do out[k] = give[k + 1] or 0 end
        return give_cond
    end)

check(not cleo5.available, 'no moonsamp.asi to load here')
cleo5._bind(fake)
check(cleo5.available, '_bind makes it available')

give_cond = 1
check(cleo5.isKeyJustPressed(0x71) == true and last.op == 0x2080 and last.types == 'i', 'isKeyJustPressed: 2080, a condition')
give = { 0x41 }
local ok, key = cleo5.getKeyPressedInRange(0x41, 0x5A)
check(ok and key == 0x41 and last.types == 'iio' and last.nout == 0, 'getKeyPressedInRange: its key from an inline variable')

cleo5.trace('%d apples and %s', 3, 'pears')
check(last.op == 0x2101 and last.types == 'sse' and sent(1, '%s') and sent(2, '3 apples and pears'),
    'trace formats in Lua and sends the text as one %s argument')
cleo5.trace('100% done')
check(sent(2, '100% done'), 'a text with no arguments is sent as it is')
cleo5.logToFile('root:\\log.txt', true, 'x=%d', 5)
check(last.types == 'sisse' and last.input[2] == 1 and sent(4, 'x=5'), 'logToFile: path, timestamp flag, text')

give = { 9 }
check(cleo5.setBit(1, 3) == 9 and last.types == 'vi' and last.input[1] == 1, 'setBit: a variable in and its new value out')
cleo5.toggleBit(0, 2, true)
check(last.types == 'vii' and last.input[3] == 1, 'toggleBit takes the state as a boolean')

give = { 20 }
check(cleo5.pickRandomInt(10, 20, 30) == 20 and last.types == 'iiioe', 'pickRandomInt: the values, the output, the end')
bits.f = 2.5
give = { bits.i }
check(cleo5.pickRandomFloat(1.5, 2.5) == 2.5 and last.types == 'ffoe', 'pickRandomFloat')
give = {}
cleo5.pickRandomText('a', 'b')
check(last.types == 'ssoe' and sent(1, 'a'), 'pickRandomText')

cleo5.isTruthy('x')
check(last.types == 's', 'isTruthy sends text as text')
cleo5.isTruthy(0)
check(last.types == 'i', 'and a number as a number')

cleo5.setAudioStreamSpeed(0x1234, 1.5)
check(last.op == 0x2503 and last.input[1] == 0x1234 and last.input[2] == fbits(1.5),
    'audio functions take a BASS channel number as it is')
cleo5.setAudioStreamVolumeWithTransition(7, 0.5, 300)
check(last.types == 'ifi' and last.input[3] == 300, 'setAudioStreamVolumeWithTransition: stream, volume, ms')

cleo5.displayTextFormatted(10, 20, 'hp %d', 100)
check(last.op == 0x2605 and last.types == 'ffsse' and sent(4, 'hp 100'), 'displayTextFormatted')
cleo5.addTextLabelFormatted('MYKEY', '%s!', 'hi')
check(last.types == 'ssse' and sent(1, 'MYKEY') and sent(3, 'hi!'), 'addTextLabelFormatted')

give, give_cond = { 0x400 }, 1
local opened, file = cleo5.openFile('root:\\a.txt', 'rb')
check(opened and file == 0x400 and last.op == 0x0A9A and last.types == 'ss', 'openFile: ok and a file')
give = { 2026, 11, 7, 1, 2, 3, 4 }
local wok, y, mo, d = cleo5.getFileWriteTime('a.txt')
check(wok and y == 2026 and mo == 11 and d == 7 and last.nout == 7, 'getFileWriteTime: seven outputs')

give = {}
local names, ops = 0, {}
for name, fn in pairs(cleo5) do
    if type(fn) == 'function' and name ~= '_bind' and name ~= 'audioStreamChannel' then
        names = names + 1
        fn(1, 1, 1, 1, 1, 1, 1, 1)
        if ops[last.op] then check(false, name .. ' runs an opcode of its own') end
        ops[last.op] = name
    end
end
local c = io.open(arg[0]:gsub('[^/]+$', '') .. '../src/cleo5.c'):read('*a')
local registered, missing = 0, {}
for hex in c:gmatch('{ 0x(%x%x%x%x), op_') do
    registered = registered + 1
    if not ops[tonumber(hex, 16)] then missing[#missing + 1] = hex end
end
check(registered > 0 and names == registered and #missing == 0,
    ('a function for each of the %d opcodes cleo5.c registers (%d, missing %s)')
        :format(registered, names, table.concat(missing, ' ')))

print(('%d passed, %d failed'):format(passed, failed))
os.exit(failed == 0 and 0 or 1)
