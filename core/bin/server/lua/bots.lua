local entity = require("entity")
local tcpproxy = require("tcpproxy")
local tabtostr = require 'tabtostr'
local int64 = require("int64")
local entitymng = require("entitymng")
local sc = require("sc")

local botsFactory = {}

function botsFactory.New()
    local obj = entity.New()
    obj:Inherit("clientmoveplugin")
    obj.status = 0
    obj.transform.position.x = 1
    obj.transform.position.y = 1
    obj.transform.position.z = 1

    obj.transform.rotation.x = 0
    obj.transform.rotation.z = 0
    obj.transform.rotation.y = 0

    obj.transform.velocity = 0
    obj.transform.stamp = os.time()
    obj.transform.stampStop = 0
    obj.entities = {}

    function obj:Init()
        self.server = tcpproxy.New(self.id)
        entitymng.RegistryUpdata(self)
    end

    function obj:Destory()
    end

    function obj:Update(count, deltaTime)
        if self.status == 0 then
            self.server:Ping(1)
        elseif self.status == 2 and os.time() > self.transform.stampStop then
            self:MoveTo(math.random(0, sc.bigworld.endx), 0, math.random(0, sc.bigworld.endz))
        end
    end

    function obj:Pong(count)
        self.status = 1
    end

    function obj:entityCall(id, fun, ...)
        if fun == 'Pong' then
            local arg = {...}
            self:Pong(arg[1])
        elseif fun == 'OnEntryWorld' then
            self:OnEntryWorld(...)
        elseif fun == 'OnAddView' then
            self:OnAddView(...)
        end
    end

    function obj:OnAddView(entity)
        local id64 = tostring(int64.new_unsigned(entity[1]))

        if self.entities[id64] == nil then
            entitymng.EntityDataCreate(id64, entity)
            self.entities[id64] = 1
        else
            entitymng.EntityDataSet(id64, entity)
        end
    end

    function obj:OnEntryWorld(poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
        obj.transform.position.x = poitionx
        obj.transform.position.y = poitiony
        obj.transform.position.z = poitionz

        obj.transform.rotation.x = rotationx
        obj.transform.rotation.z = rotationy
        obj.transform.rotation.y = rotationz

        obj.transform.velocity = velocity
        obj.transform.stamp = stamp
        obj.transform.stampStop = stampStop

        self:MoveTo(math.random(0, sc.bigworld.endx), 0, math.random(0, sc.bigworld.endz))
        self.status = 2
    end
    return obj
end

return botsFactory