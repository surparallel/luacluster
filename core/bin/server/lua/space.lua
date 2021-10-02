--space
local entity = require("entity")
local entitymng = require("entitymng")
local cmsgpack = require("cmsgpack")
local docker = require("docker")
local int64 = require("int64")
local elog = require("elog")
local udpproxy = require 'udpproxy'

local spaceFactory= {}

function spaceFactory.New()
    local obj = entity.New()
    obj.entities = {}

    --注册自己的entity id到redis
    function obj:Init()

        elog.fun("space::init")
        entitymng.RegistrySev(self.ServerName, self)
        self.spaceType = "space"
        --entitymng.RegistryUpdata(self)
    end
    
    function obj:Update(count, deltaTime)

        --print("space update", count, deltaTime)
    end

    function obj:EntryWorld(id, poitionx, poitionz, rotationy, velocity, stamp, stampStop, isGhost)
        local entryid = tostring(int64.new_unsigned(id))

        elog.fun("space::LeaveWorld"..entryid)
        if self.entities[entryid] ~= nil then
            elog.n_error("EntryWorld::Repeat into space error".. entryid)
            return
        end

        --从redis获取对象并调用空间的EntryWorld
        docker.Send(id, cmsgpack.pack("OnAddView", self.entities))
        
        for key, value in pairs(self.entities) do
            local keyid = int64.new_unsigned(key)
            docker.Send(keyid.tonumber(keyid), cmsgpack.pack("OnAddView", {[entryid]=self.entities[entryid]}))
        end

        obj.entities[entryid] = {}
        obj.entities[entryid][1] = id
        obj.entities[entryid][2] = poitionx
        obj.entities[entryid][3] = poitionz
        obj.entities[entryid][4] = rotationy
        obj.entities[entryid][5] = velocity
        obj.entities[entryid][6] = stamp
        obj.entities[entryid][7] = stampStop

        --这个空间没有尺寸限制
        local entityProxy = udpproxy.New(id)
        entityProxy:OnEntryWorld("space")

        local entityProxy = udpproxy.New(id)
        entityProxy:OnGetSpace(self.id)
    end

    function obj:LeaveWorld(id)
        --从redis获取对象并调用空间的LeaveWorld  
        local entryid = tostring(int64.new_unsigned(id))
        
        elog.fun("space::LeaveWorld"..entryid)
        if self.entities[entryid] == nil then
            elog.n_error("LeaveWorld::level from space no find id error".. entryid)
            return
        end

        --从redis获取对象并调用空间的EntryWorld
        for key, value in pairs(self.entities) do
            local keyid = int64.new_unsigned(key)
            docker.Send(keyid.tonumber(keyid), cmsgpack.pack("OnDelView", {[entryid]=0}))
        end

        self.entities[entryid] = nil
    end
    
    function obj:Destory()
    end

    return obj
end

return spaceFactory