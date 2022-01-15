local entity = require("entity")
local tcpproxy = require("tcpproxy")
local sc = require("sc")
local docker = require("docker")
local int64 = require("int64")
local elog = require("eloghelp")

local accountFactory = {}

function accountFactory.OnFreshKey(t,k,v,o,f)
    if t:HaveKeyFlags(k, sc.keyflags.private) then
        docker.SendToClient(t.id, t.id, cmsgpack.pack("ChangStatus", {[k]=v}))
    end
end

function accountFactory.New()
    local obj = entity.New()
    obj:Inherit("spaceplugin")
    obj:AddKeyFlags("Ping", sc.keyflags.exposed)
    obj:AddKeyFlags("Move", sc.keyflags.exposed)
    obj.client = tcpproxy.New(obj.id)

    obj:AddFlagFun(sc.keyflags.private, accountFactory.OnFreshKey)

    obj.transform = {}
    obj.transform.position = {}
    obj.transform.position.x = 1
    obj.transform.position.y = 1
    obj.transform.position.z = 1

    obj.transform.rotation = {}
    obj.transform.rotation.x = 0
    obj.transform.rotation.z = 0
    obj.transform.rotation.y = 0

    obj.transform.velocity = 0
    obj.transform.stamp = os.time()
    obj.transform.stampStop = 0

    function obj:Init()

        local myid = tostring(int64.new_unsigned(self.id))
        elog.sys_fun("account::init %s", myid)
        if(obj.clientid ~= nil)then
            obj.client = tcpproxy.New(self.id)
        end
    end

    function obj:Destory()
        obj:LeaveWorld("bigworld")
    end
    
    function obj:Ping(a)
        obj.client:Pong(a)

        --收到ping表示客户端初始化完成进入世界
        obj:EntryWorld("bigworld")
    end

    function obj:OnEntryWorld(spaceType, beginx, beginz, endx, endz)
        self.spaceInfo.spaceType = spaceType
        self.spaceInfo.beginx = beginx
        self.spaceInfo.beginz = beginz
        self.spaceInfo.endx = endx
        self.spaceInfo.endz = endz

        --通知客户端进入世界
        self.client:OnEntryWorld(self.transform.position.x, self.transform.position.y, self.transform.position.z
        , self.transform.rotation.x
        , self.transform.rotation.y
        , self.transform.rotation.z
        , self.transform.velocity
        , self.transform.stamp
        , self.transform.stampStop)
    end

    function obj:Move(poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
        obj.transform.position.x = poitionx
        obj.transform.position.y = poitiony
        obj.transform.position.z = poitionz

        obj.transform.rotation.x = rotationx
        obj.transform.rotation.y = rotationy
        obj.transform.rotation.z = rotationz

        obj.transform.velocity = velocity
        obj.transform.stamp = stamp
        obj.transform.stampStop = stampStop

        self:SpaceMove(self.transform.position.x, self.transform.position.y, self.transform.position.z
        ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity, self.transform.stamp, self.transform.stampStop);
    end

    return obj
end

return accountFactory