--moveplugin
local docker = require("docker")
local int64 = require("int64")
local udpproxy = require 'udpproxy'
local math3d = require 'math3d'
local elog = require("eloghelp")
local entity = require("entity")
local sc = require("sc")

local clientMovepluginFactory = {}

function clientMovepluginFactory.New()
    local obj = entity.New()
    obj.transform = {}
    obj.transform.position = {}
    obj.transform.nowPosition = {}
    obj.transform.rotation = {}

    function obj:UpdatePosition()
        self.transform.nowPosition.x, self.transform.nowPosition.y, self.transform.nowPosition.z = math3d.Position(
        self.transform.position.x, self.transform.position.y, self.transform.position.z
        , self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z
        , self.transform.velocity, self.transform.stamp, self.transform.stampStop)
    end

    function obj:MoveTo(x, y, z)

        if (x < sc.bigworld.beginx or  x > sc.bigworld.endx) and (z < sc.bigworld.beginz or  z > sc.bigworld.endz) then
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
        elog.sys_fun("clientMoveplugin(%s)::MoveTo (x:%f y:%f z:%f)=>(x:%f y:%f z:%f) d:%f b:%f e:%f rx:%f ry:%f rz:%f"
        ,tostring(myid)
        , self.transform.position.x, self.transform.position.y, self.transform.position.z
        , x, y, z, d, self.transform.stamp, self.transform.stampStop
        , self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z)

        self.server:Move(self.transform.position.x, self.transform.position.y, self.transform.position.z
        , self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z
        , self.transform.velocity, self.transform.stamp, self.transform.stampStop)
    end

    return obj
end

return clientMovepluginFactory