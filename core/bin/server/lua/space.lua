--space
local entity = require("entity")
local entitymng = require("entitymng")
local cmsgpack = require("cmsgpack")
local docker = require("docker")
local int64 = require("int64")
local elog = require("eloghelp")
local udpproxy = require 'udpproxy'

local spaceFactory= {}

function spaceFactory.New()
    local obj = entity.New()
    obj.entities = {}

    --注册自己的entity id到redis
    function obj:Init()

        elog.sys_fun("space::init")
        entitymng.RegistrySev(self.ServerName, self)
        self.spaceType = "space"
        --entitymng.RegistryUpdata(self)
    end
    
    function obj:Update(count, deltaTime)

        --print("space update", count, deltaTime)
    end

    function obj:EntryWorld(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost)
        local entryid = tostring(int64.new_unsigned(id))

        elog.sys_fun("space::LeaveWorld %s",entryid)
        if self.entities[entryid] ~= nil then
            elog.n_error(string.format("EntryWorld::Repeat into space error %s", entryid))
            return
        end

        --从redis获取对象并调用空间的EntryWorld
        docker.Send(id, cmsgpack.pack("OnAddView", self.entities))
        
        for key, value in pairs(self.entities) do
            local keyid = int64.new_unsigned(key)
            docker.Send(keyid.tonumber(keyid), cmsgpack.pack("OnAddView", {[entryid]=self.entities[entryid]}))
        end

        self.entities[entryid] = {}
        self.entities[entryid][1] = id
        self.entities[entryid][2] = poitionx
        self.entities[entryid][3] = poitiony
        self.entities[entryid][4] = poitionz
        self.entities[entryid][5] = rotationx
        self.entities[entryid][6] = rotationy
        self.entities[entryid][7] = rotationz
        self.entities[entryid][8] = velocity
        self.entities[entryid][9] = stamp
        self.entities[entryid][10] = stampStop

        --这个空间没有尺寸限制
        local entityProxy = udpproxy.New(id)
        entityProxy:OnEntryWorld("space")

        local entityProxy = udpproxy.New(id)
        entityProxy:OnGetSpace(self.id)
    end

    function obj:LeaveWorld(id)
        --从redis获取对象并调用空间的LeaveWorld  
        local entryid = tostring(int64.new_unsigned(id))
        
        elog.sys_fun("space::LeaveWorld"..entryid)
        if self.entities[entryid] == nil then
            elog.n_error(string.format("LeaveWorld::level from space no find id error %s", entryid))
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