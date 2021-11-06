local mongo = require 'mongo'
local BSON = mongo.BSON

local testInt64 = math.maxinteger and math.maxinteger == 9223372036854775807

local function testV(v1, v2, h)
	local a1 = v1.a
	local b1, b2 = BSON(v1), BSON(v2)
	assert(b1 == b2) -- Compare with overloaded equality operator
	assert(b1:data() == b2:data()) -- Compare binary data
	assert(tostring(b1) == tostring(b2)) -- Compare as strings
	test.equal(b1:value(h), b2:value(h)) -- Compare as values
	if a1 then -- Test nested value
		local a2 = b1:find('a')
		b1, b2 = BSON{}, BSON{}
		b1:append('a', a1)
		b2:append('a', a2)
		test.equal(b1:value(h), b2:value(h))
	end
	collectgarbage()
end

local function testX(v, h)
	local b = BSON(v)
	local v_ = b:value(h)
	local b_ = BSON(v_)
	assert(b == b_)
	test.equal(v, v_)
	collectgarbage()
end

local function testF(v)
	test.failure(BSON, v)
end


-- BSON()
local b1 = BSON{a = 1, b = {__array = true, 1, 2, 3}}
local b2 = BSON(b1:data())
assert(b1 == b2)

-- bson:concat()
local b1 = BSON{a = 1}
local b2 = BSON{b = 2}
b1:concat(b2)
testV(b1, '{ "a" : 1, "b" : 2 }')
b1:concat(b2)
testV(b1, '{ "a" : 1, "b" : 2, "b" : 2 }')
test.failure(b1.concat, b1) -- Invalid value

-- bson:find()
local b = BSON{a = {b = mongo.Null}}
assert(b:find('') == nil)
assert(b:find('abc') == nil)
assert(b:find('a.b') == mongo.Null)


-- Arrays

local function a(n)
	local t = {__array = true}
	for i = 1, n do
		t[#t + 1] = i
	end
	if n < 50 then
		t[#t + 1] = a(n + 1)
	end
	return t
end

local a = {a = a(1)}
local b = BSON(a)
assert(b == BSON(b:value())) -- Nested arrays
local a0 = {__array = 0}
local a1 = {__array = 3, 1, 2, 3}
local a0_ = {a0 = a0}
local a1_ = {a1 = a1}
testV(a0, '[  ]') -- Empty root array
testV(a1, '[ 1, 2, 3 ]') -- Root array
testV(a0_, '{ "a0" : [  ] }') -- Empty nested array
testV(a1_, '{ "a1" : [ 1, 2, 3 ] }') -- Nested array
-- testX(a0) -- Empty root array is not transitive due to limitations of libbson
testX(a1)
testX(a0_)
testX(a1_)


-- Values

testV({a = true}, '{ "a" : true }')
testV({a = 'abc'}, '{ "a" : "abc" }')
testV({a = {__array = 6, nil, 1, nil, 2, nil}}, '{ "a" : [ null, 1, null, 2, null, null ] }') -- Extended array
testV({b = {__array = -1, 1, 2, 3}}, '{ "b" : [  ] }') -- Truncated array
testV({a = {b = 1}}, '{ "a" : { "b" : 1 } }')

testV({a = 2147483647}, '{ "a" : 2147483647 }') -- Max Int32
testV({a = -2147483648}, '{ "a" : -2147483648 }') -- Min Int32
testV({a = 1.7976931348623157e+308}, '{ "a" : 1.7976931348623157e+308 }') -- Max Double
testV({a = -1.7976931348623157e+308}, '{ "a" : -1.7976931348623157e+308 }') -- Min Double
if testInt64 then
	testV({a = 9223372036854775807}, '{ "a" : { "$numberLong" : "9223372036854775807" } }') -- Max Int64
	testV({a = -9223372036854775808}, '{ "a" : { "$numberLong" : "-9223372036854775808" } }') -- Min Int64
else
	print 'Max Int64 testing skipped - no support on this system'
end


-- Types

testV({a = BSON{b = 1}}, '{ "a" : { "b" : 1 } }') -- Nested document
testV({a = mongo.Int32(1234)}, '{ "a" : 1234 }')
testV({a = mongo.Int64(1234)}, '{ "a" : { "$numberLong" : "1234" } }')
testV({a = mongo.Double(10)}, '{ "a" : 10.0 }')
testV({a = mongo.Decimal128('-10.123')}, '{ "a" : { "$numberDecimal" : "-10.123" } }')
testV({a = mongo.Binary('abc')}, '{ "a" : { "$binary" : "YWJj", "$type" : "0" } }')
testV({a = mongo.Binary('abc', 0x80)}, '{ "a" : { "$binary" : "YWJj", "$type" : "80" } }')
if testInt64 then -- DateTime as Int64
	testV({a = mongo.DateTime(9223372036854775807)}, '{ "a" : { "$date": { "$numberLong" : "9223372036854775807" } } }')
	testV({a = mongo.DateTime(-9223372036854775808)}, '{ "a" : { "$date": { "$numberLong" : "-9223372036854775808" } } }')
else -- DateTime as Double
	testV({a = mongo.DateTime(9007199254740991)}, '{ "a" : { "$date": { "$numberLong" : "9007199254740991" } } }')
	testV({a = mongo.DateTime(-9007199254740992)}, '{ "a" : { "$date": { "$numberLong" : "-9007199254740992" } } }')
end
testV({a = mongo.Javascript('abc')}, '{ "a" : { "$code" : "abc" } }')
testV({a = mongo.Javascript('abc', {a = 1})}, '{ "a" : { "$code" : "abc", "$scope" : { "a" : 1 } } }')
testV({a = mongo.Regex('abc')}, '{ "a" : { "$regex" : "abc", "$options" : "" } }')
testV({a = mongo.Regex('abc', 'def')}, '{ "a" : { "$regex" : "abc", "$options" : "def" } }')
testV({a = mongo.Timestamp(4294967295, 4294967295)}, '{ "a" : { "$timestamp" : { "t" : 4294967295, "i" : 4294967295 } } }')
testV({a = mongo.MaxKey}, '{ "a" : { "$maxKey" : 1 } }')
testV({a = mongo.MinKey}, '{ "a" : { "$minKey" : 1 } }')
testV({a = mongo.Null}, '{ "a" : null }')


-- Handlers

local mt = {__toBSON = function (t) return {bin = mongo.Binary(t.str)} end}
local obj = setmetatable({str = 'abc'}, mt)
local h1 = function (t) return setmetatable({str = t.bin:unpack()}, mt) end
local h2 = function (t) return t.a and t or h1(t) end
testV(obj, '{ "bin" : { "$binary" : "YWJj", "$type" : "0" } }', h1) -- Root transformation
testV({a = obj}, '{ "a" : { "bin" : { "$binary" : "YWJj", "$type" : "00" } } }', h2) -- Nested transformation
testX(obj, h1) -- Root transition
testX({a = obj}, h2) -- Nested transition


-- Errors

testF(setmetatable({}, {})) -- Table with metatable
testF(setmetatable({}, {__toBSON = function (t) return {t = t} end})) -- Recursion in '__toBSON'
testF(setmetatable({}, {__toBSON = function (t) return 'abc' end})) -- Root '__toBSON' should return table or BSON
testF(setmetatable({}, {__toBSON = function (t) t() end})) -- Run-time error in root '__toBSON'
testF{a = setmetatable({}, {__toBSON = function (t) t() end})} -- Run-time error in nested '__toBSON'

local t = {}
t.t = t
testF(t) -- Circular reference
local f = function () end
testF{a = f} -- Invalid value
testF{[f] = 1} -- Invalid key
testF('') -- Empty data
testF('abc') -- Invalid BSON/JSON
testF{__array = false, 1, 2, 3} -- Not an array

local function newBSONType(n, ...)
	return setmetatable({...}, {__type = n})
end

testF{a = newBSONType(0x99)} -- Invalid type
testF{a = newBSONType(0x05)} -- Binary: invalid string
testF{a = newBSONType(0x0d)} -- Javascript: invalid string
testF{a = newBSONType(0x13)} -- Decimal128: invalid string
testF{a = newBSONType(0x13, 'abc')} -- Decimal128: invalid format
test.failure(mongo.Decimal128, 'abc') -- Invalid format


-- ObjectID

local oid1 = mongo.ObjectID('000000000000000000000000')
local oid2 = mongo.ObjectID('000000000000000000000000')
assert(oid1 == oid2) -- Compare with overloaded equality operator
assert(oid1:data() == oid1:data()) -- Compare binary data
assert(oid1:hash() == oid2:hash()) -- Compare hashes
assert(tostring(oid1) == tostring(oid2)) -- Compare as strings
testV({a = oid1}, '{ "a" : { "$oid" : "000000000000000000000000" } }')
test.failure(mongo.ObjectID, 'abc') -- Invalid format
local oid = mongo.ObjectID() -- Random ObjectID
assert(oid ~= oid1 and oid:data() ~= oid1:data() and tostring(oid) ~= tostring(oid1)) -- Compare everything except hashes
