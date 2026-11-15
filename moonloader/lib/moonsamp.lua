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

-- MoonLoader's AudioStream object keeps its BASS channel at +8 (0.26:
-- 0x1012BE60, 027: 0x1012A050); the userdata reaches it differently in each
-- build. 026's path is tried first, then the userdata's first words as the
-- object or a pointer to it; a channel counts when BASS knows the handle.
-- Anything that is not a userdata is taken as a channel already.
local u32p = ffi.typeof('uint32_t *')
pcall(ffi.cdef, [[
int BASS_ChannelIsActive(uint32_t handle);
int BASS_ErrorGetCode(void);
int IsBadReadPtr(const void *p, uintptr_t n);
]])
local bass
do
    local ok, lib = pcall(ffi.load, 'bass.dll')
    if ok then bass = lib end
end

local function read(at)
    if not at or at == 0 or ffi.C.IsBadReadPtr(ffi.cast('void *', at), 4) ~= 0 then return nil end
    return tonumber(ffi.cast(u32p, at)[0])
end

local function is_channel(c)
    if not c or c == 0 or not bass then return false end
    bass.BASS_ChannelIsActive(c)
    return bass.BASS_ErrorGetCode() == 0
end

local function from_object(obj) return obj and read(obj + 8) end

local function candidates(a)
    local c = a + 4 + (-a) % 4
    local d = c + (-c) % 4
    local list = { function() local p = read(d + (-(d + 4)) % 4 + 4); return from_object(read(p)) end }
    for k = 0, 7 do
        list[#list + 1] = function() return from_object(a + 4 * k) end
        list[#list + 1] = function() return from_object(read(a + 4 * k)) end
        list[#list + 1] = function() return from_object(read(read(a + 4 * k))) end
    end
    return list
end

local stream_layout     -- the candidate that worked last, tried first
function M.channel(stream)
    if type(stream) ~= 'userdata' then return stream end
    local list = candidates(tonumber(ffi.cast('uintptr_t', ffi.cast('void *', stream))))
    if stream_layout then
        local c = list[stream_layout]()
        if is_channel(c) then return c end
    end
    for k, f in ipairs(list) do
        local c = f()
        if is_channel(c) then
            stream_layout = k
            return c
        end
    end
    return 0
end

-- target[name] runs op on an audio stream, its first argument.
function M.define_audio(target, name, op, inputs, outputs, cond)
    target[name] = function(stream, ...) return M.call(op, inputs, outputs, cond, M.channel(stream), ...) end
end

-- target[name] runs op.
function M.define(target, name, op, inputs, outputs, cond)
    target[name] = function(...) return M.call(op, inputs, outputs, cond, ...) end
end

return M
