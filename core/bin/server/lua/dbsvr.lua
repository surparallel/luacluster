--dbsvr
local sc = require 'sc'
local entitymng = require("entitymng")
local elog = require("eloghelp")
local udpproxy = require 'udpproxy'
local int64 = require("int64")
local mongo = require 'mongo'

---@class DBSvr
local dbsvr = class()
dbsvr.ServerName = "dbsvr"

--注册自己的entity id到redis
function dbsvr:Init()
    elog.sys_fun("dbsvr::init %i", _G["bots"])
    --在客户端模式下不运行
    if(_G["bots"] == 1) then
        return
    end

    entitymng.RegistrySev(self.ServerName, self)
    --entitymng.RegistryUpdata(self)
    self.client = mongo.Client(sc.db.uri)
    self.database = self.client:getDatabase(sc.db.dbname)
    local r, e = self.database:createCollection(sc.db.collname)
    if e ~= nil then
        --主意需要运维介入的严重数据库错误
        elog.sys_warn(e)
    end

    self.collection = self.client:getCollection(sc.db.dbname, sc.db.collname)
end

function dbsvr:Save(entityid, date)
    local myid = tostring(int64.new_unsigned(entityid))
    local r, e = self.collection:updateOne('{"entityid":\"'.. myid ..'\"}', '{ "$set":'.. date ..'}', '{"upsert":true}')
    if e ~= nil then
        elog.sys_warn(e)
    end
    local cursor, ef = self.collection:findOne({entityid = myid})
    if ef ~= nil then
        elog.sys_warn(ef)
    end
    local entityProxy = udpproxy(entityid)
    entityProxy:SaveBack(tostring(cursor:value()._id))
end

function dbsvr:SaveUpdate(dbid, date)
    local r, e = self.collection:updateOne('{"_id":{ "$oid":\"'.. dbid ..'\" }}', '{ "$set":'.. date ..'}', '{"upsert":true}')
    if e ~= nil then
        elog.sys_warn(e)
    end
end

function dbsvr:Load(dbid, entityid)
    local cursor, e = self.collection:findOne('{"_id":{ "$oid":\"'.. dbid ..'\" }}')
    if e ~= nil then
        elog.sys_warn(e)
    end
    local entityProxy = udpproxy(entityid)
    entityProxy:LoadBack(tostring(cursor))
end

function dbsvr:LoadAccount(account, password, entityid)
    local cursor, e = self.collection:findOne('{"account":\"'.. account ..'\", "password":\"'.. password ..'\"}')
    if e ~= nil then
        elog.sys_warn(e)
    end
    local entityProxy = udpproxy(entityid)
    entityProxy:LoadAccountBack(tostring(cursor))
end

function dbsvr:Update(count, deltaTime)
end

--理论上这个对象不会被销毁
function dbsvr:Destory()

end

return dbsvr
