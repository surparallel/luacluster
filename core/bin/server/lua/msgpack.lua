--[[----------------------------------------------------------------------------

  MessagePack encoder / decoder written in pure Lua 5.3
  written by Sebastian Steinhauer <s.steinhauer@yahoo.de>

  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org/>

--]]----------------------------------------------------------------------------
local msgpack = {
  _AUTHOR = 'Sebastian Steinhauer <s.steinhauer@yahoo.de>',
  _VERSION = '0.5.1',
}


--[[----------------------------------------------------------------------------
      Speed optimizations
--]]----------------------------------------------------------------------------
local type = type
local pairs = pairs
local pcall = pcall
local select = select
local table = require('table')
local string = require('string')
local math = require('math')
local utf8 = require('utf8')


--[[----------------------------------------------------------------------------
      DECODING
--]]----------------------------------------------------------------------------
local decoder_table -- forward reference

local function unpack(ctx, fmt)
  local value, position = fmt:unpack(ctx.input, ctx.position)
  ctx.position = position
  return value
end

local function decode_next(ctx)
  return decoder_table[unpack(ctx, '>B')](ctx)
end

local function decode_array(ctx, length)
  local new = {}
  for i = 1, length do
    new[i] = decode_next(ctx)
  end
  return new
end

local function decode_map(ctx, length)
  local new = {}
  for i = 1, length do
    local k = decode_next(ctx)
    local v = decode_next(ctx)
    new[k] = v
  end
  return new
end

--[[ Decoder Table ]]-----------------------------------------------------------
decoder_table = {
  [0xc0] = function() return nil end,
  [0xc2] = function() return false end,
  [0xc3] = function() return true end,
  [0xc4] = function(ctx) return unpack(ctx, '>s1') end,
  [0xc5] = function(ctx) return unpack(ctx, '>s2') end,
  [0xc6] = function(ctx) return unpack(ctx, '>s4') end,
  [0xca] = function(ctx) return unpack(ctx, '>f') end,
  [0xcb] = function(ctx) return unpack(ctx, '>d') end,
  [0xcc] = function(ctx) return unpack(ctx, '>I1') end,
  [0xcd] = function(ctx) return unpack(ctx, '>I2') end,
  [0xce] = function(ctx) return unpack(ctx, '>I4') end,
  [0xcf] = function(ctx) return unpack(ctx, '>I8') end,
  [0xd0] = function(ctx) return unpack(ctx, '>i1') end,
  [0xd1] = function(ctx) return unpack(ctx, '>i2') end,
  [0xd2] = function(ctx) return unpack(ctx, '>i4') end,
  [0xd3] = function(ctx) return unpack(ctx, '>i8') end,
  [0xd9] = function(ctx) return unpack(ctx, '>s1') end,
  [0xda] = function(ctx) return unpack(ctx, '>s2') end,
  [0xdb] = function(ctx) return unpack(ctx, '>s4') end,
  [0xdc] = function(ctx) return decode_array(ctx, unpack(ctx, '>I2')) end,
  [0xdd] = function(ctx) return decode_array(ctx, unpack(ctx, '>I4')) end,
  [0xde] = function(ctx) return decode_map(ctx, unpack(ctx, '>I2')) end,
  [0xdf] = function(ctx) return decode_map(ctx, unpack(ctx, '>I4')) end,
}

-- add single byte integers
for i = 0x00, 0x7f do
  decoder_table[i] = function() return i end
end
for i = 0xe0, 0xff do
  decoder_table[i] = function() return -32 + i - 0xe0 end
end

-- add fixed maps
for i = 0x80, 0x8f do
  decoder_table[i] = function(ctx) return decode_map(ctx, i - 0x80) end
end

-- add fixed arrays
for i = 0x90, 0x9f do
  decoder_table[i] = function(ctx) return decode_array(ctx, i - 0x90) end
end

-- add fixed strings
for i = 0xa0, 0xbf do
  local format = string.format('>c%d', i - 0xa0)
  decoder_table[i] = function(ctx) return unpack(ctx, format) end
end


--[[----------------------------------------------------------------------------
      ENCODING
--]]----------------------------------------------------------------------------
local encoder_table -- forward reference

local function encode_value(value)
  return encoder_table[type(value)](value)
end

local function check_array(value) -- simple function to verify a table is a proper array
  local expected = 1
  for k in pairs(value) do
    if k ~= expected then return false end
    expected = expected + 1
  end
  return true
end

--[[ Encoder Table ]]-----------------------------------------------------------
encoder_table = {
  ['nil'] = function()
    return ('>B'):pack(0xc0)
  end,

  ['boolean'] = function(value)
    return ('>B'):pack(value and 0xc3 or 0xc2)
  end,

  ['string'] = function(value)
    local length = #value
    if utf8.len(value) then -- valid UTF-8 ... encode as string
      if length < 32 then
        return ('>B'):pack(0xa0 + length) .. value
      elseif length <= 0xff then
        return ('>B s1'):pack(0xd9, value)
      elseif length <= 0xffff then
        return ('>B s2'):pack(0xda, value)
      else
        return ('>B s4'):pack(0xdb, value)
      end
    else -- encode as binary
      if length <= 0xff then
        return ('>B s1'):pack(0xc4, value)
      elseif length <= 0xffff then
        return ('>B s2'):pack(0xc5, value)
      else
        return ('>B s4'):pack(0xc6, value)
      end
    end
  end,

  ['number'] = function(value)
    if math.type(value) == 'integer' then
      if value >= 0 then
        if value <= 0x7f then
          return ('>B'):pack(value)
        elseif value <= 0xff then
          return ('>B I1'):pack(0xcc, value)
        elseif value <= 0xffff then
          return ('>B I2'):pack(0xcd, value)
        elseif value <= 0xffffffff then
          return ('>B I4'):pack(0xce, value)
        else
          return ('>B I8'):pack(0xcf, value)
        end
      else
        if value >= -32 then
          return ('>B'):pack(0xe0 + value + 32)
        elseif value >= -127 then
          return ('>B i1'):pack(0xd0, value)
        elseif value >= -32767 then
          return ('>B i2'):pack(0xd1, value)
        elseif value >= -2147483647 then
          return ('>B i4'):pack(0xd2, value)
        else
          return ('>B i8'):pack(0xd3, value)
        end
      end
    else
      local converted = ('>f'):unpack(('>f'):pack(value)) -- check if we can use 32-bit floats
      if converted == value then
        return ('>B f'):pack(0xca, value)
      else
        return ('>B d'):pack(0xcb, value)
      end
    end
  end,

  ['table'] = function(value)
    if check_array(value) then
      local elements = {}
      for i, v in pairs(value) do
        elements[i] = encode_value(v)
      end

      local length = #elements
      if length <= 0xf then
        return ('>B'):pack(0x90 + length) .. table.concat(elements)
      elseif length <= 0xffff then
        return ('>B I2'):pack(0xdc, length) .. table.concat(elements)
      else
        return ('>B I4'):pack(0xdd, length) .. table.concat(elements)
      end
    else
      local elements = {}
      for k, v in pairs(value) do
        elements[#elements + 1] = encode_value(k)
        elements[#elements + 1] = encode_value(v)
      end

      local length = #elements // 2
      if length <= 0xf then
        return ('>B'):pack(0x80 + length) .. table.concat(elements)
      elseif length <= 0xffff then
        return ('>B I2'):pack(0xde, length) .. table.concat(elements)
      else
        return ('>B I4'):pack(0xdf, length) .. table.concat(elements)
      end
    end
  end,
}


--[[----------------------------------------------------------------------------
      PUBLIC API
--]]----------------------------------------------------------------------------
function msgpack.encode_one(value)
  local ok, result = pcall(encode_value, value)
  if ok then
    return result
  else
    return nil, string.format('cannot encode value "%s" to MessagePack', type(value))
  end
end

function msgpack.encode(...)
  local result = {}
  for i = 1, select('#', ...) do
    local data, err = msgpack.encode_one(select(i, ...))
    if data then
      result[#result + 1] = data
    else
      return nil, err
    end
  end
  return table.concat(result)
end

function msgpack.decode_one(input, position)
  local ctx = { input = input, position = position or 1 }
  local ok, result = pcall(decode_next, ctx)
  if ok then
    return result, ctx.position
  else
    return nil, 'cannot decode MessagePack'
  end
end

function msgpack.decode(input, position)
  local ctx = { input = input, position = position or 1 }
  local result = {}
  while ctx.position <= #ctx.input do
    local ok, value = pcall(decode_next, ctx)
    if ok then
      result[#result + 1] = value
    else
      return nil, 'cannot decode MessagePack'
    end
  end
  return table.unpack(result)
end

return msgpack
--[[----------------------------------------------------------------------------
--]]----------------------------------------------------------------------------
