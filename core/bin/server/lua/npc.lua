---@type Entity
local entity = require("entity")
local sc = require("sc")
local docker = require("docker")
local int64 = require("int64")
local entitymng = require("entitymng")
---@type TcpProxy
local tcpproxy = require("tcpproxy")

---@class Npc
local npc = class("moveplugin")

function npc.OnFreshKey(root, rootKey)
    print("OnFreshKey")
end

npc:AddFlagFun(sc.keyflags.private, npcFactory.OnFreshKey)

npc.transform.position.x = 0
npc.transform.position.y = 0
npc.transform.position.z = 0

npc.transform.rotation.x = 0
npc.transform.rotation.z = 0
npc.transform.rotation.y = 0

npc.transform.velocity = 0
npc.transform.stamp = os.time()
npc.transform.stampStop = 0

npc.status = 0

function npc:Init()

    elog.sys_fun("npc::init")
    entitymng.RegistryUpdata(self)

    if(self.clientid ~= nil)then
        self.client = tcpproxy(self.id)
    end

    self:EntryWorld("bigworld")
end

function npc:OnGetSpace(spaceId)

    --移动到地图的对角
    if self.status == 0 then
        self:MoveTo(self.spaceInfo.endx, 0, self.spaceInfo.endz)
        self.status = 1
    end
end

function npc:Destory()
    npc:LeaveWorld("bigworld")
end

function npc:Update(count, deltaTime)
    self.__allparant["spaceplugin"].Update(self, count, deltaTime)

    if self.status == 1 then
        if os.time() > self.transform.stampStop and self.spaceInfo.endx ~= nil and self.spaceInfo.endz ~= nil then
            self:MoveTo(math.random(0, self.spaceInfo.endx), 0, math.random(0, self.spaceInfo.endz))
        end
    end
end

return npc