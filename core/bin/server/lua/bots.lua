---@type Entity
local entity = require("entity")
---@type TcpProxy
local tcpproxy = require("tcpproxy")
local tabtostr = require 'tabtostr'
local int64 = require("int64")
local entitymng = require("entitymng")
local sc = require("sc")

---@class Bots
local bots = class("clientmoveplugin")

bots.status = 0
bots.transform.position.x = 1
bots.transform.position.y = 1
bots.transform.position.z = 1

bots.transform.rotation.x = 0
bots.transform.rotation.z = 0
bots.transform.rotation.y = 0

bots.transform.velocity = 0
bots.transform.stamp = os.time()
bots.transform.stampStop = 0
bots.entities = {}

function bots:Init()
    self.server = tcpproxy(self.id)
    entitymng.RegistryUpdata(self)
end

function bots:Destory()
    entitymng.UnRegistryUpdata(self)
    entitymng.UnRegistryObj(self.id)
end

function bots:Update(count, deltaTime)
    if self.status == 0 then
        self.server:Ping(1)
    elseif self.status == 2 and os.time() > self.transform.stampStop then
        self:MoveTo(math.random(0, sc.bigworld.endx), 0, math.random(0, sc.bigworld.endz))
    end
end

function bots:Pong(count)
    self.status = 1
end

function bots:entityCall(id, fun, ...)
    if fun == 'Pong' then
        local arg = {...}
        self:Pong(arg[1])
    elseif fun == 'OnEntryWorld' then
        self:OnEntryWorld(...)
    elseif fun == 'OnAddView' then
        self:OnAddView(...)
    end
end

function bots:OnAddView(entity)
    local id64 = tostring(int64.new_unsigned(entity[1]))

    self.entities[id64] = 1
    entitymng.EntityDataSet(id64, entity)
end

function bots:OnEntryWorld(poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
    bots.transform.position.x = poitionx
    bots.transform.position.y = poitiony
    bots.transform.position.z = poitionz

    bots.transform.rotation.x = rotationx
    bots.transform.rotation.z = rotationy
    bots.transform.rotation.y = rotationz

    bots.transform.velocity = velocity
    bots.transform.stamp = stamp
    bots.transform.stampStop = stampStop

    self:MoveTo(math.random(0, sc.bigworld.endx), 0, math.random(0, sc.bigworld.endz))
    self.status = 2
end

return bots