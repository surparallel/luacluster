local entity = require("entity")
local entitymng = require("entitymng")
local cmsgpack = require("cmsgpack")
local docker = require("docker")
local int64 = require("int64")
local elog = require("elog")

local spaceFactory= {}

function spaceFactory.New(arg)
    print("New space")
    local obj = entity.New(arg)
    obj.entities = {}

    --注册自己的entity id到redis
    function obj:Init()
        entitymng.RegistrySev(self.ServerName, self)
        --entitymng.RegistryUpdata(self)
    end
    
    function obj:update(count, deltaTime)

        --print("space update", count, deltaTime)
    end

    function obj:EntryWorld(id)
        
        local entryid = tostring(int64.new_unsigned(id))

        if self.entities[entryid] ~= nil then
            elog.n_error("EntryWorld::Repeat into space error".. entryid)
            return
        end

        --从redis获取对象并调用空间的EntryWorld
        docker.Send(id, cmsgpack.pack("OnAddView", self.entities))
        
        for key, value in pairs(self.entities) do
            local keyid = int64.new_unsigned(key)
            docker.Send(keyid, cmsgpack.pack("OnAddView", {[entryid] = 0}))
        end

        self.entities[entryid] = 1--这里记录相关移动状态
    end

    function obj:LeaveWorld(id)
        --从redis获取对象并调用空间的LeaveWorld

        local entryid = tostring(int64.new_unsigned(id))

        if self.entities[entryid] == nil then
            elog.n_error("LeaveWorld::level from space no find id error".. entryid)
            return
        end

        --从redis获取对象并调用空间的EntryWorld
        for key, value in pairs(self.entities) do
            local keyid = int64.new_unsigned(key)
            docker.Send(keyid, cmsgpack.pack("OnDelView", {[entryid] = 0}))
        end

        self.entities[entryid] = nil--这里记录相关移动状态
    end
    
    function obj:Destory()
    end

    return obj
end

return spaceFactory