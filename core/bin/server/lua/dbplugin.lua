--dbplugin
local docker = require("docker")
local int64 = require("int64")
local elog = require("eloghelp")
local entity = require("entity")
local entitymng = require("entitymng")
local sc = require("sc")
local json = require("dkjson")

local dbPluginFactory = {}

function dbPluginFactory.OnFreshKey(root, rootKey)
    root:AddFresh(rootKey)
end

function dbPluginFactory.New()
    local obj = entity.New()
    obj:AddFlagFun(sc.keyflags.persistent, dbPluginFactory.OnFreshKey)

    function  obj:Save()

        if self.dbid ~= nil then
            local myid = tostring(int64.new_unsigned(self.id))
            elog.sys_error("dbplugin Save error Object %s already has dbid %s",myid,self.dbid)
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

    function  obj:SaveUpdate()

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

    function  obj:Load(dbid)

        if self.dbsvr == nil then
            self.dbsvr = entitymng.GetSev("dbsvr")
        end
        self.dbid = dbid
        self.dbsvr:Load(self.dbid, self.id)

    end

    function  obj:AddFresh(key)
        local FreshKey = rawget(self, "__FreshKey")
        table.insert(FreshKey, key)
    end

    function  obj:SaveBack(dbid)
        self.dbid = dbid
    end

    function  obj:LoadBack(data)
        local jdata = json.decode(data, nil, nil, nil, nil)
        jdata["entityid"] = nil
        jdata["_id"] = nil
        for k, v in pairs(jdata) do
            self[k] = v
        end
        rawset(self, "__FreshKey", {})
    end

    return obj
end

return dbPluginFactory