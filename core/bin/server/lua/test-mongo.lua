local mongo = require 'mongo'
local client = mongo.Client(test.uri)

-- Read prefs
local prefs = mongo.ReadPrefs('primary', nil, -1)
test.failure(mongo.ReadPrefs, 'abc') -- Invalid mode
test.failure(mongo.ReadPrefs, 'primary', {}, 90) -- 'primary' may not have 'maxStalenessSeconds'

-- Regular types
assert(mongo.type(nil) == 'nil')
assert(mongo.type('abc') == 'string')
test.failure(mongo.type)


-- Collection

local collection = client:getCollection(test.dbname, test.collname)
assert(collection:getName() == test.collname)
assert(mongo.type(collection:getReadPrefs()) == 'mongo.ReadPrefs')
collection:setReadPrefs(prefs)
collection:drop()

test.error(collection:insert({['$a'] = 123})) -- Client-side error
test.error(collection:insert({['$a'] = 123}, {noValidate = true})) -- Server-side error

assert(collection:insert{_id = 123})
test.error(collection:insert{_id = 123}) -- Duplicate key
assert(collection:insert{_id = 456})
assert(collection:insert{_id = 789})

assert(collection:count{} == 3)
assert(collection:count('{ "_id" : 123 }') == 1)
assert(collection:count{_id = {['$gt'] = 123}} == 2)
assert(collection:count({}, {skip = 1, limit = 2}) == 2) -- Options

-- cursor:next()
local cursor = collection:find{} -- Find all
assert(cursor:more())
assert(mongo.type(cursor:next()) == 'mongo.BSON') -- #1
assert(mongo.type(cursor:next()) == 'mongo.BSON') -- #2
assert(mongo.type(cursor:next()) == 'mongo.BSON') -- #3
local r, e = cursor:next()
assert(r == nil and e == nil) -- nil + no error
assert(not cursor:more())
r, e = cursor:next()
assert(r == nil and type(e) == 'string') -- nil + error
collectgarbage()

-- cursor:value()
cursor = collection:find{_id = 123}
assert(cursor:value()._id == 123)
assert(cursor:value() == nil) -- No more items
test.failure(cursor.value, cursor) -- Exception is thrown
cursor = collection:find{_id = 123}
assert(cursor:value(function (t) return {id = t._id} end).id == 123) -- With transformation
collectgarbage()

-- cursor:iterator()
local i, s = collection:find('{ "_id" : { "$gt" : 123 } }', {sort = {_id = -1}}):iterator() -- _id > 123, desc order
local v1 = assert(i(s))
local v2 = assert(i(s))
assert(v1._id == 789)
assert(v2._id == 456)
assert(i(s) == nil) -- No more items
test.failure(i, s) -- Exception is thrown
i, s = collection:find({_id = 123}):iterator(function (t) return {id = t._id} end) -- With transformation
assert(i(s).id == 123)
collectgarbage()

assert(collection:remove({}, {single = true})) -- Flags
assert(collection:count{} == 2)
assert(collection:remove{_id = 123})
assert(collection:remove{_id = 123}) -- Remove reports 'true' even if not found
assert(collection:findOne({_id = 123}) == nil) -- Not found

assert(collection:update({_id = 123}, {a = 'abc'}, {upsert = true})) -- inSERT
assert(collection:update({_id = 123}, {a = 'def'}, {upsert = true})) -- UPdate
assert(collection:findOne({_id = 123}):value().a == 'def')

assert(collection:findAndModify({_id = 123}, {update = {a = 'abc'}}):find('a') == 'def') -- Old value
assert(collection:findAndModify({_id = 'abc'}, {remove = true}) == nil) -- Not found

assert(collection:aggregate('[ { "$group" : { "_id" : "$a", "count" : { "$sum" : 1 } } } ]'):value().count == 1)

-- *One/*Many operations
local t = {}
for i = 1, 100 do
	t[#t + 1] = {a = 1}
end
test.failure(collection.insertMany, collection, (table.unpack or unpack)(t)) -- Too many documents
test.error(collection:insertMany()) -- Empty insert
test.error(collection:insertMany({_id = 123}, {_id = 456})) -- Duplicate key
collection:drop()
for a = 1, 3 do
	assert(collection:insertOne{a = a})
end
assert(collection:insertMany({a = 4}, {a = 5}, {a = 6}))
assert(collection:count{} == 6)
assert(collection:removeMany('{ "a" : { "$gt" : 4 } }'))
assert(collection:removeOne{})
assert(collection:replaceOne({}, {b = 1}))
assert(collection:updateMany({}, '{ "$inc" : { "b" : 1 } }'))
assert(collection:updateOne({}, '{ "$inc" : { "b" : 1 } }'))
local cursor = collection:find{}
assert(cursor:value().b == 3)
assert(cursor:value().b == 1)
assert(cursor:value().b == 1)
assert(cursor:value() == nil)

-- Bulk operation
local function bulkInsert(ordered, n)
	collection:drop()
	local bulk = collection:createBulkOperation{ordered = ordered}
	for id = 1, n do
		bulk:insert{_id = id}
		bulk:insert{_id = id}
	end
	test.error(bulk:execute()) -- Errors about duplicate keys
	return collection:count{}
end
assert(bulkInsert(false, 3) == 3) -- Unordered insert
assert(bulkInsert(true, 3) == 1) -- Ordered insert

collection:drop()
local bulk = collection:createBulkOperation()
for a = 1, 6 do
	bulk:insert{a = a}
end
bulk:removeMany('{ "a" : { "$gt" : 4 } }')
bulk:removeOne{}
bulk:replaceOne({}, {b = 1})
bulk:updateMany({}, '{ "$inc" : { "b" : 1 } }')
bulk:updateOne({}, '{ "$inc" : { "b" : 1 } }')
assert(bulk:execute())
local cursor = collection:find{}
assert(cursor:value().b == 3)
assert(cursor:value().b == 1)
assert(cursor:value().b == 1)
assert(cursor:value() == nil)

-- Rename collection
assert(collection:rename(test.dbname, tostring(mongo.ObjectID()))) -- Rename with arbitrary name
local newCollection = collection
collection = client:getCollection(test.dbname, test.collname) -- Recreate a testing collection
assert(collection:insert{a = 1}) -- Insert something to create the actual storage
assert(newCollection:rename(test.dbname, test.collname, true)) -- Rename back with force
newCollection = nil

collection = nil
collectgarbage()


-- Database

local database = client:getDatabase(test.dbname)
assert(database:getName() == test.dbname)
assert(mongo.type(database:getReadPrefs()) == 'mongo.ReadPrefs')
database:setReadPrefs(prefs)

assert(database:removeAllUsers())
assert(database:addUser(test.dbname, 'pwd'))
test.error(database:addUser(test.dbname, 'pwd'))
assert(database:removeUser(test.dbname))
test.error(database:removeUser(test.dbname))

test.value(assert(database:getCollectionNames()), test.collname)
assert(database:hasCollection(test.collname))

test.error(database:createCollection(test.collname)) -- Collection already exists
collection = database:getCollection(test.collname)
collection:drop()
collection = assert(database:createCollection(test.collname, {capped = true, size = 1024})) -- Create collection explicitly
collection = nil

database = nil
collectgarbage()


-- Client

test.failure(mongo.Client, 'abc') -- Invalid URI format

-- client:getDefaultDatabase()
local c1 = mongo.Client('mongodb://aaa')
local c2 = mongo.Client('mongodb://aaa/bbb')
test.failure(c1.getDefaultDatabase, c1) -- No default database in URI
assert(c2:getDefaultDatabase():getName() == 'bbb')

test.value(assert(client:getDatabaseNames()), test.dbname)
assert(mongo.type(client:getReadPrefs()) == 'mongo.ReadPrefs')
client:setReadPrefs(prefs)

-- client:command()
assert(mongo.type(assert(client:command(test.dbname, {find = test.collname}))) == 'mongo.Cursor') -- client:command() returns cursor
assert(mongo.type(assert(client:command(test.dbname, {validate = test.collname}))) == 'mongo.BSON') -- client:command() returns BSON
test.error(client:command('abc', {INVALID_COMMAND = test.collname}))

-- Cleanup
assert(client:getDatabase(test.dbname):drop())
