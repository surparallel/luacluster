---@type Entity
local entity = require("entity")
local int64 = require("int64")
local sc = require("sc")
local json = require("dkjson")

---@class DBEntity
local dbEntity = class("dbplugin")

dbEntity:AddKeyFlags(sc.keyflags.persistent, "a")
dbEntity:AddKeyFlags( sc.keyflags.persistent, "b")
dbEntity:AddKeyFlags(sc.keyflags.persistent, "c")
dbEntity:AddKeyFlags(sc.keyflags.persistent, "account")
dbEntity:AddKeyFlags(sc.keyflags.persistent, "passwrod")
dbEntity.stamp = os.time()

function dbEntity:Init()
    local myid = tostring(int64.new_unsigned(self.id))
    print("Init dbentity:",myid)
end

function dbEntity:fun()
    print("dbentity fun")
    self.account = 'admin'
    self.password = '123456'
    self.a= 1
    self.b = {a = 1, b = 2}
    self.c = {1,2,3,4}
    table.remove(self.c, 1)
    self:Save()
end

function dbEntity:fun3()
    print("dbentity fun2")
    self:LoadAccount('admin', '123456')
end

function  dbEntity:SaveBack(dbid)
    self.__allparant["dbplugin"].SaveBack(self,dbid)
    print("dbentity id",dbid)
    self.b = {a = 3, b = 4}
    self:SaveUpdate()
    self:Load(dbid)
end

function  dbEntity:LoadAccountBack(data)

    if data == nil then
        print("db not find "..self.account..":"..self.password)
        return
    end

    print(data)
    self.__allparant["dbplugin"].LoadBack(self, data)
end

function  dbEntity:LoadBack(data)

    if data == nil then
        print("db not find "..self.dbid)
        return
    end

    print(data)
    self.__allparant["dbplugin"].LoadBack(self, data)
end

function dbEntity:fun2(a, b, c)
    print("dbentity fun2:",a," ",b,"",c)
end

function dbEntity:mongo()
    require(assert(("test"), 'test script is missing'))
end

function dbEntity:Update(count, deltaTime)
    --定期保存
    if self.stamp - os.time() > sc.db.updateTime then
    self:SaveUpdate()
    end
end

return dbEntity