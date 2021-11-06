local mongo = require 'mongo'
local client = mongo.Client(test.uri)
local gridfs = assert(client:getGridFS(test.dbname))
local chunks = gridfs:getChunks()
local files = gridfs:getFiles()
gridfs:drop()

local t = {}
for i = 1, 50000 do
	t[#t + 1] = ('%s'):format(i % 10)
end
local data = table.concat(t)


-- GridFS file

local file = assert(gridfs:createFile())

assert(file:getAliases() == nil)
assert(file:getChunkSize() > 0)
assert(file:getContentType() == nil)
assert(file:getFilename() == nil)
assert(mongo.type(file:getId()) == 'mongo.ObjectID')
assert(file:getLength() == 0)
assert(file:getMD5() == nil)
assert(file:getMetadata() == nil)
assert(file:getUploadDate() > 1486000000)

file:setAliases('[ "a", "b" ]')
file:setContentType('content-type1')
file:setFilename('myfile1')
file:setId(123)
file:setMD5('abc')
file:setMetadata('{ "a" : 1, "b" : 2 }')

assert(file:save())

assert(tostring(file:getAliases()) == '{ "0" : "a", "1" : "b" }')
assert(file:getContentType() == 'content-type1')
assert(file:getFilename() == 'myfile1')
assert(file:getId() == 123)
assert(file:getMD5() == 'abc')
assert(tostring(file:getMetadata()) == '{ "a" : 1, "b" : 2 }')

assert(file:write(data) == #data)
assert(file:read(#data) == nil) -- Reading past the end -> EOF
assert(file:seek(0))
assert(file:tell() == 0)
assert(file:read(#data) == data) -- Reading from the beginning
assert(file:tell() == #data)

file = nil
collectgarbage()

file = assert(gridfs:createFile{
	aliases = '[ "a", "b", "c" ]',
	chunkSize = 10000,
	contentType = 'content-type2',
	filename = 'myfile2',
	md5 = 'def',
	metadata = '{ "a" : 1, "b" : 2, "c" : 3 }',
})

assert(tostring(file:getAliases()) == '{ "0" : "a", "1" : "b", "2" : "c" }')
assert(file:getChunkSize() == 10000)
assert(file:getContentType() == 'content-type2')
assert(file:getFilename() == 'myfile2')
assert(file:getMD5() == 'def')
assert(tostring(file:getMetadata()) == '{ "a" : 1, "b" : 2, "c" : 3 }')

-- Write in chunks
local m, n = 0, 256
while m < #data do
	local d = data:sub(m + 1, n)
	assert(file:write(d) == #d)
	m = m + #d
	n = n * 2
end

assert(file:read(#data) == nil) -- Reading past the end -> EOF
assert(file:seek(0))
assert(file:tell() == 0)

-- Read in chunks
local m, n = 0, 256
while true do
	local d, e = file:read(n)
	if d == nil then
		assert(e == nil) -- No error, plain EOF
		break
	end
	m = m + #d
	n = n * 2
end
assert(m == #data)
assert(file:tell() == #data)
assert(file:seek(10, 'end'))
assert(file:tell() == #data + 10)
assert(file:seek(10, 'cur'))
assert(file:tell() == #data + 20)

assert(not file:seek(-10)) -- Invalid 'offset'
test.failure(file.seek, file, 10, 'aaa') -- Invalid 'whence'

assert(chunks:count{} == 5)
assert(files:count{} == 2)

-- list:next()
local list = gridfs:find{} -- Find all
assert(mongo.type(list:next()) == 'mongo.GridFSFile') -- #1
assert(mongo.type(list:next()) == 'mongo.GridFSFile') -- #2
local r, e = list:next()
assert(r == nil and e == nil) -- nil + no error
r, e = list:next()
assert(r == nil and type(e) == 'string') -- nil + error
collectgarbage()

-- list:iterator()
local i, s = gridfs:find({}, {sort = {filename = 1}}):iterator()
local f1 = assert(i(s))
local f2 = assert(i(s))
assert(f1:getFilename() == 'myfile1')
assert(f2:getFilename() == 'myfile2')
assert(i(s) == nil) -- No more items
test.failure(i, s) -- Exception is thrown
collectgarbage()

assert(file:remove())

-- gridfs:createFileFrom()
test.error(gridfs:createFileFrom('NON-EXISTENT-FILE'))
local f = io.open(test.filename, 'w')
if f then
	f:write(data)
	f:close()
	file = assert(gridfs:createFileFrom(test.filename, {filename = 'myfile3'}))
	os.remove(test.filename)
	assert(file:read(#data) == data)
	assert(file:tell() == #data)
	assert(file:remove())
else
	print 'gridfs:createFileFrom() testing skipped - unable to create local file'
end

file = nil
collectgarbage()


-- GridFS

assert(gridfs:findOne{_id = 123})
assert(gridfs:findOne{filename = 'myfile2'} == nil)
assert(gridfs:findOneByFilename('myfile1'))
assert(gridfs:findOneByFilename('myfile2') == nil)
assert(gridfs:removeByFilename('myfile1'))

assert(chunks:count{} == 0)
assert(files:count{} == 0)
assert(gridfs:drop())

local c = mongo.Client('mongodb://INVALID-URI')
test.error(c:getGridFS(test.dbname))
