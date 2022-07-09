test = {
	uri = 'mongodb://127.0.0.1',
	dbname = 'lua-mongo-test',
	collname = 'test',
	filename = 'lua-mongo-test-file',
}

function test.error(s, e)
	assert(not s, 'status is OK')
	assert(type(e) == 'string', 'error is not a string')
	print('test passed', e)
end

function test.failure(f, ...)
	assert(type(f) == 'function', 'not a function')
	local s, e = pcall(f, ...)
	assert(not s, 'call succeeded')
	print('test passed', e)
end

local function compare(v1, v2)
	if type(v1) ~= 'table' or type(v2) ~= 'table' then
		return v1 == v2
	end
	if v1 == v2  then
		return true
	end
	if not compare(getmetatable(v1), getmetatable(v2)) then
		return false
	end
	for k, v in pairs(v1) do
		if not compare(v, v2[k]) then
			return false
		end
	end
	for k, v in pairs(v2) do
		if not compare(v, v1[k]) then
			return false
		end
	end
	return true
end

function test.equal(v1, v2)
	assert(compare(v1, v2), 'values differ')
end

function test.key(t, x)
	for k in pairs(t) do
		if compare(k, x) then
			return
		end
	end
	error 'key not found'
end

function test.value(t, x)
	for _, v in pairs(t) do
		if compare(v, x) then
			return
		end
	end
	error 'value not found'
end

--require(assert(("test-bson"), 'test script is missing'))
--require(assert(("test-gridfs"), 'test script is missing'))
require(assert(("test-mongo"), 'test script is missing'))
