local entity = require("entity")
local tcpproxy = require("tcpproxy")
local sc = require("sc")
local docker = require("docker")
local int64 = require("int64")
local entitymng = require("entitymng")

local npcFactory = {}

function npcFactory.New()
    local obj = entity.New()
    obj:Inherit("moveplugin")
    obj.client = tcpproxy.New(obj.id)

    obj.transform.poition.x = 0
    obj.transform.poition.y = 0
    obj.transform.poition.z = 0

    obj.transform.rotation.x = 0
    obj.transform.rotation.z = 0
    obj.transform.rotation.y = 0

    obj.transform.velocity = 3
    obj.transform.stamp = os.time()
    obj.transform.stampStop = 0

    obj.status = 0

    function obj:Init()

        elog.fun("npc::init")
        entitymng.RegistryUpdata(self)

        if(obj.clientid ~= nil)then
            obj.client = tcpproxy.New(self.id)
        end

        obj:EntryWorld("bigworld")
    end

    function obj:OnGetSpace(spaceId)

        --移动到地图的对角
        if obj.status == 0 then
            self:MoveTo(self.spaceInfo.endx, 0, self.spaceInfo.endz)
            obj.status = 1
        end
    end

    function obj:Destory()
        obj:LeaveWorld("bigworld")
    end

    function obj:Update(count, deltaTime)
        self.__allparant["spaceplugin"].Update(self, count, deltaTime)
    end

    return obj
end

return npcFactory