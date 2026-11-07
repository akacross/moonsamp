--[[ moonsamp.asi's opcode runner for Lua: newopcodes.lua and cleo5.lua are
built on it. MoonLoader cannot run an opcode it has no function for;
moonsamp_run writes the parameters in SCM format and runs moonsamp's own
handler, with or without CLEO.

Input letters: i an int, handle or pointer, b a boolean, f a float, s a
string, x a string or a number (a pointer to text, or an index), v an int
variable the opcode reads and writes. Letters that take no argument: o a
variable the opcode writes, e the end of a variable argument list. The
values of the v and o variables come back first, then the outputs after
them. Output letters: i an int, u an unsigned int (pointers), f a float, b a
boolean, s the text a char * points at. ]]
local ffi = require 'ffi'

pcall(ffi.cdef, 'int moonsamp_run(int op, const char *types, const int32_t *in, int nout, int32_t *out);')

local M = {}
local run
local ins, outs = ffi.new('int32_t[48]'), ffi.new('int32_t[48]')
local bits = ffi.new('union { float f; int32_t i; }')
local charp = ffi.typeof('const char *')

do
    local ok, lib = pcall(ffi.load, 'moonsamp.asi')
    if ok then
        ok, run = pcall(function() return lib.moonsamp_run end)
        if not ok then run = nil end
    end
end
M.available = run ~= nil

-- For tests: a C function (or ffi callback) with moonsamp_run's signature.
function M._bind(fn)
    run = fn
    M.available = fn ~= nil
end

local function i32(v)
    if v == nil or v == false then return 0 end
    if v == true then return 1 end
    if type(v) == 'cdata' then v = tonumber(ffi.cast('uintptr_t', v)) end
    v = math.floor(v) % 4294967296
    return v >= 2147483648 and v - 4294967296 or v
end
M.i32 = i32

local function decode(t, raw)
    raw = tonumber(raw)
    if t == 'f' then
        bits.i = raw
        return tonumber(bits.f)
    elseif t == 'u' then
        return raw % 4294967296
    elseif t == 'b' then
        return raw ~= 0
    elseif t == 's' then
        return raw ~= 0 and ffi.string(ffi.cast(charp, raw % 4294967296)) or nil
    end
    return raw
end

-- Runs op: inputs as above, outputs one letter per value it returns.
-- Strings stay in `keep` until the call is over.
function M.call(op, inputs, outputs, cond, ...)
    if not run then error('moonsamp.asi with moonsamp_run is not loaded', 3) end
    local args, sig, keep, a, vars = { ... }, {}, {}, 0, 0
    for k = 1, #inputs do
        local t = inputs:sub(k, k)
        if t == 'o' or t == 'e' then
            ins[k - 1] = 0
            if t == 'o' then vars = vars + 1 end
            sig[k] = t
        else
            a = a + 1
            local v = args[a]
            if t == 'f' then
                bits.f = v or 0
                ins[k - 1] = bits.i
                sig[k] = 'f'
            elseif t == 's' or (t == 'x' and type(v) == 'string') then
                keep[k] = tostring(v or '')
                ins[k - 1] = i32(ffi.cast('uintptr_t', ffi.cast(charp, keep[k])))
                sig[k] = 's'
            else
                ins[k - 1] = i32(v)
                if t == 'v' then vars = vars + 1 end
                sig[k] = t == 'v' and 'v' or 'i'
            end
        end
    end
    local result = run(op, table.concat(sig), ins, #outputs - vars, outs) ~= 0
    local r = {}
    for k = 1, #outputs do r[k] = decode(outputs:sub(k, k), outs[k - 1]) end
    if cond then return result, unpack(r, 1, #outputs) end
    return unpack(r, 1, #outputs)
end

-- target[name] runs op.
function M.define(target, name, op, inputs, outputs, cond)
    target[name] = function(...) return M.call(op, inputs, outputs, cond, ...) end
end

return M
