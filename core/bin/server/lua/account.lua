local entity = require("entity")
local tcpproxy = require("tcpproxy")
local sc = require("sc")
local docker = require("docker")
local int64 = require("int64")
local elog = require("eloghelp")
local entitymng = require("entitymng")
local udpproxylist = require 'udpproxylist'

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
        --正常流程要加延迟处理，否则在注册空间过程中退出会导致空间残留
        obj:LeaveWorld("bigworld")
        --通知管理器销毁对象，当对象有多个继承时要考虑销毁顺序
        entitymng.UnRegistryObj(self.id)
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
        ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity, 
        self.transform.stamp, self.transform.stampStop);

        local list = docker.CreateMsgList()
        for k, v1 in pairs(self.entities) do
            local v = entitymng.EntityDataGet(k)
            if(v ~= nil)then
                local view = udpproxylist.New(v[1], list)
                view:OnMove(self.id, self.transform.position.x, self.transform.position.y, self.transform.position.z
                ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity
                , self.transform.stamp, self.transform.stampStop)
            end
        end
        docker.PushAllMsgList(list)
        docker.DestoryMsgList(list)
    end

    return obj
end

return accountFactory