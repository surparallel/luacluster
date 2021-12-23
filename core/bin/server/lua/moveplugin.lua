--moveplugin
local docker = require("docker")
local int64 = require("int64")
local udpproxy = require 'udpproxy'
local math3d = require 'math3d'
local elog = require("eloghelp")
local entity = require("entity")

local movepluginFactory = {}

function movepluginFactory.New()
    local obj = entity.New()
    obj:Inherit("spaceplugin")

    function obj:UpdatePosition()
        self.transform.nowPotion.x, self.transform.nowPotion.y, self.transform.nowPotion.z = math3d.Position(
        self.transform.poition.x, self.transform.poition.y, self.transform.poition.z
        , self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z
        , self.transform.velocity, self.transform.stamp, self.transform.stampStop)
    end

    function obj:MoveTo(x, y, z)

        if (x < self.spaceInfo.beginx or  x > self.spaceInfo.endx) and (z < self.spaceInfo.beginz or  z > self.spaceInfo.endz) then
            elog.error("space::MoveTo outside error x:%f y:%f z:%f",x,y,z)
            return
        end

        self:UpdatePosition()
        local d, rx, ry, rz = math3d.LookVector(self.transform.nowPotion.x, self.transform.nowPotion.y, self.transform.nowPotion.z, x, y, z)

        self.transform.poition.x = self.transform.nowPotion.x
        self.transform.poition.y = self.transform.nowPotion.y
        self.transform.poition.z = self.transform.nowPotion.z
        self.transform.rotation.x = rx
        self.transform.rotation.y = ry
        self.transform.rotation.z = rz
        self.transform.stamp = os.time()
        self.transform.stampStop = self.transform.stamp + d / self.transform.velocity

        local myid = int64.new_unsigned(self.id)
        elog.fun("spaceplugin(%s)::MoveTo (x:%f y:%f z:%f)=>(x:%f y:%f z:%f) d:%f b:%f e:%f rx:%f ry:%f rz:%f"
        ,tostring(myid)
        , self.transform.poition.x, self.transform.poition.y, self.transform.poition.z
        , x, y, z, d, self.transform.stamp, self.transform.stampStop
        , self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z)

        for k, v in pairs(self.entities) do
            local entity = udpproxy.New(v[1])
            entity:OnMove(self.id, self.transform.poition.x, self.transform.poition.y, self.transform.poition.z
            ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity, self.transform.stamp, self.transform.stampStop);
        end

        for k, v in pairs(self.spaces) do
            v:Move(self.id, self.transform.poition.x, self.transform.poition.y, self.transform.poition.z
            ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity, self.transform.stamp, self.transform.stampStop);
        end
    end

    return obj
end

return movepluginFactory