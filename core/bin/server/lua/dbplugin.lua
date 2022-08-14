--dbplugin
local int64 = require("int64")
local elog = require("eloghelp")
local entitymng = require("entitymng")
local sc = require("sc")
local json = require("dkjson")

---@class DBPlugin
local dbplugin = class()

function dbplugin.OnFreshKey(root, rootKey)
    root:AddFresh(rootKey)
end

dbplugin:AddFlagFun(sc.keyflags.persistent, dbplugin.OnFreshKey)

function  dbplugin:Save()

    if self.dbid ~= nil then
        local myid = tostring(int64.new_unsigned(self.id))
        elog.sys_error("dbplugin Save error dbpluginect %s already has dbid %s",myid,self.dbid)
        return
    end

    if self.dbsvr == nil then
        self.dbsvr = entitymng.GetSev("dbsvr")
    end

    local FreshKey = rawget(self, "__FreshKey")
    local out = {}
    for i, v in ipairs(FreshKey) do
        if type(self[v]) ~= "table" then
            out[v] = self[v]
        else
            self:Recombination(self[v], out)
        end
    end

    rawset(self, "__FreshKey", {})
    local str = json.encode(out)
    self.dbsvr:Save(self.id, str)

end

function  dbplugin:SaveUpdate()

    if self.dbid == nil then
        return
    end

    if self.dbsvr == nil then
        self.dbsvr = entitymng.GetSev("dbsvr")
    end

    local FreshKey = rawget(self, "__FreshKey")
    local out = {}
    for i, v in ipairs(FreshKey) do
        if type(self[v]) ~= "table" then
            out[v] = self[v]
        else
            self:Recombination(self[v], out)
        end
    end

    rawset(self, "__FreshKey", {})
    local str = json.encode(out)
    self.dbsvr:SaveUpdate(self.dbid, str)

end

function  dbplugin:Load(dbid)

    if self.dbsvr == nil then
        self.dbsvr = entitymng.GetSev("dbsvr")
    end
    self.dbid = dbid
    self.dbsvr:Load(self.dbid, self.id)

end

function  dbplugin:LoadAccount(account, password)

    if self.dbsvr == nil then
        self.dbsvr = entitymng.GetSev("dbsvr")
    end
    self.account = account
    self.password = password
    self.dbsvr:LoadAccount(account, password, self.id)

end

function  dbplugin:AddFresh(key)
    local FreshKey = rawget(self, "__FreshKey")
    table.insert(FreshKey, key)
end

function  dbplugin:SaveBack(dbid)
    self.dbid = dbid
end

function  dbplugin:LoadBack(data)
    local jdata = json.decode(data, nil, nil, nil, nil)
    jdata["entityid"] = nil
    jdata["_id"] = nil
    for k, v in pairs(jdata) do
        self[k] = v
    end
    rawset(self, "__FreshKey", {})
end

return dbplugin
