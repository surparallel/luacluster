--moveplugin
local docker = require("docker")
local int64 = require("int64")
local udpproxylist = require 'udpproxylist'
local math3d = require 'math3d'
local elog = require("eloghelp")
local entity = require("entity")
local entitymng = require("entitymng")

local movepluginFactory = {}

function movepluginFactory.New()
    local obj = entity.New()
    obj:Inherit("spaceplugin")

    function obj:UpdatePosition()
        self.transform.nowPosition.x, self.transform.nowPosition.y, self.transform.nowPosition.z = math3d.Position(
        self.transform.position.x, self.transform.position.y, self.transform.position.z
        , self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z
        , self.transform.velocity, self.transform.stamp, self.transform.stampStop)
    end

    function obj:MoveTo(x, y, z)

        if (x < self.spaceInfo.beginx or  x > self.spaceInfo.endx) and (z < self.spaceInfo.beginz or  z > self.spaceInfo.endz) then
            elog.sys_error("space::MoveTo outside error x:%f y:%f z:%f",x,y,z)
            return
        end

        self:UpdatePosition()
        local d, rx, ry, rz = math3d.LookVector(self.transform.nowPosition.x, 0, self.transform.nowPosition.z, x, y, z)

        self.transform.position.x = self.transform.nowPosition.x
        self.transform.position.y = 0
        self.transform.position.z = self.transform.nowPosition.z
        self.transform.rotation.x = rx
        self.transform.rotation.y = ry
        self.transform.rotation.z = rz
        self.transform.velocity = 2.0
        self.transform.stamp = os.time()
        self.transform.stampStop = self.transform.stamp + d / self.transform.velocity

        local myid = int64.new_unsigned(self.id)
        elog.sys_fun("spaceplugin(%s)::MoveTo (x:%f y:%f z:%f)=>(x:%f y:%f z:%f) d:%f b:%f e:%f rx:%f ry:%f rz:%f"
        ,tostring(myid)
        , self.transform.position.x, self.transform.position.y, self.transform.position.z
        , x, y, z, d, self.transform.stamp, self.transform.stampStop
        , self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z)

        local list = docker.CreateMsgList()
        for k, v1 in pairs(self.entities) do
            local v = entitymng.EntityDataGet(k)
            if v ~= nil then
                local view = udpproxylist.New(v[1], list)
                view:OnMove(self.id, self.transform.position.x, self.transform.position.y, self.transform.position.z
                ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity
                , self.transform.stamp, self.transform.stampStop)
            end
        end
        docker.PushAllMsgList(list)
        docker.DestoryMsgList(list)

        --调用spalceplugin得方法通知所有空间
        self:SpaceMove(self.transform.position.x, self.transform.position.y, self.transform.position.z
        ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity, self.transform.stamp, self.transform.stampStop);

    end

    return obj
end

return movepluginFactory