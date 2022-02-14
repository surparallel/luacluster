local entity = require("entity")
local sc = require("sc")
local docker = require("docker")
local int64 = require("int64")
local entitymng = require("entitymng")

local npcFactory = {}
function npcFactory.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")
end

function npcFactory.New()
    local obj = entity.New()
    obj:Inherit("moveplugin")
    obj:AddFlagFun(sc.keyflags.private, npcFactory.OnFreshKey)

    obj.transform.position.x = 0
    obj.transform.position.y = 0
    obj.transform.position.z = 0

    obj.transform.rotation.x = 0
    obj.transform.rotation.z = 0
    obj.transform.rotation.y = 0

    obj.transform.velocity = 0
    obj.transform.stamp = os.time()
    obj.transform.stampStop = 0

    obj.status = 0

    function obj:Init()

        elog.sys_fun("npc::init")
        entitymng.RegistryUpdata(self)

        if(self.clientid ~= nil)then
            self.client = tcpproxy.New(self.id)
        end

        self:EntryWorld("bigworld")
    end

    function obj:OnGetSpace(spaceId)

        --移动到地图的对角
        if self.status == 0 then
            self:MoveTo(self.spaceInfo.endx, 0, self.spaceInfo.endz)
            self.status = 1
        end
    end

    function obj:Destory()
        obj:LeaveWorld("bigworld")
    end

    function obj:Update(count, deltaTime)
        self.__allparant["spaceplugin"].Update(self, count, deltaTime)

        if self.status == 1 then
            if os.time() > self.transform.stampStop and self.spaceInfo.endx ~= nil and self.spaceInfo.endz ~= nil then
                self:MoveTo(math.random(0, self.spaceInfo.endx), 0, math.random(0, self.spaceInfo.endz))
            end
        end
    end

    return obj
end

return npcFactory