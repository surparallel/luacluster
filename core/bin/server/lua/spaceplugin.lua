--spaceplugin
local sc = require 'sc'
local entitymng = require("entitymng")
local cmsgpack = require("cmsgpack")
local docker = require("docker")
local int64 = require("int64")
local spaceproxy = require 'spaceproxy'

local spacePluginFactory = {}

function spacePluginFactory.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")

    if t:HaveKeyFlags(k, sc.keyflags.broadcast) then
        for k, v in pairs(t.entities) do
            local keyid = int64.new_unsigned(k)
            docker.SendToClient(t.id, keyid, cmsgpack.pack("ChangStatus", {[k] = v}))
        end
    end
end

function spacePluginFactory.New()
    local obj = {}

    function obj:EntryWorld(sapceName)
        --从redis获取对象并调用空间的EntryWorld
        local space = entitymng.GetSev(sapceName)
        space:EntryWorld(self.id)
        self.clients = spaceproxy.New(self.id, space)
    end

    function obj:LeaveWorld(sapceName)
        --从redis获取对象并调用空间的LeaveWorld
        local space = entitymng.GetSev(sapceName)
        space:LeaveWorld(self.id)
        self.clients = nil
    end

    function obj:OnAddView(entityList)
        --从redis获取对象并调用空间的LeaveWorld
        for key, value in pairs(entityList) do
            -- body
            obj.entities[key] = value
        end

        --转发到客户端
        docker.CopyRpcToClient()
    end

    function obj:OnDelView(entityList)
        --从redis获取对象并调用空间的LeaveWorld
        for key, value in pairs(entityList) do
            -- body
            obj.entities[key] = nil
        end

        --转发到客户端
        docker.CopyRpcToClient()
    end

    return obj
end

return spacePluginFactory