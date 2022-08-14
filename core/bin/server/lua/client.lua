---@type Entity
local entity = require("entity")
---@type TcpProxy
local tcpproxy = require("tcpproxy")
local sc = require("sc")
local docker = require("docker")
local int64 = require("int64")
local elog = require("eloghelp")
local entitymng = require("entitymng")
---@type UdpProxyList
local udpproxylist = require 'udpproxylist'
local cmsgpack = require("cmsgpack")

---@class Client
local client = class("spaceplugin")

function client.OnFreshKey(root, rootKey)
    local out = nil
    root:Recombination(root[rootKey], out)
    root.client:ChangStatus(out)
end

client:AddKeyFlags(sc.keyflags.exposed, "Ping")
client:AddKeyFlags(sc.keyflags.exposed, "Move")
client.client = tcpproxy(client.id)

client:AddFlagFun(sc.keyflags.private, client.OnFreshKey)

client.transform = {}
client.transform.position = {}
client.transform.position.x = 1
client.transform.position.y = 1
client.transform.position.z = 1

client.transform.rotation = {}
client.transform.rotation.x = 0
client.transform.rotation.z = 0
client.transform.rotation.y = 0

client.transform.velocity = 0
client.transform.stamp = os.time()
client.transform.stampStop = 0

function client:Init()

    local myid = tostring(int64.new_unsigned(self.id))
    elog.sys_fun("client::init %s", myid)
    if(self.clientid ~= nil)then
        self.client = tcpproxy(self.id)
    end
end

function client:Destory()
    --正常流程要加延迟处理，否则在注册空间过程中退出会导致空间残留
    self:LeaveWorld("bigworld")
    --通知管理器销毁对象，当对象有多个继承时要考虑销毁顺序
    entitymng.UnRegistryObj(self.id)
end

function client:Ping(a)
    self.client:Pong(a)

    --收到ping表示客户端初始化完成进入世界
    self:EntryWorld("bigworld")
end

function client:OnEntryWorld(spaceType, beginx, beginz, endx, endz)
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

function client:Move(poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
    self.transform.position.x = poitionx
    self.transform.position.y = poitiony
    self.transform.position.z = poitionz

    self.transform.rotation.x = rotationx
    self.transform.rotation.y = rotationy
    self.transform.rotation.z = rotationz

    self.transform.velocity = velocity
    self.transform.stamp = stamp
    self.transform.stampStop = stampStop

    self:SpaceMove(self.transform.position.x, self.transform.position.y, self.transform.position.z
    ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity, 
    self.transform.stamp, self.transform.stampStop);

    local list = docker.CreateMsgList()
    for k, v1 in pairs(self.entities) do
        local v = entitymng.EntityDataGet(k)
        if(v ~= nil)then
            local view = udpproxylist(v[1], list)
            view:OnMove(self.id, self.transform.position.x, self.transform.position.y, self.transform.position.z
            ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity
            , self.transform.stamp, self.transform.stampStop)
        end
    end
    docker.PushAllMsgList(list)
    docker.DestoryMsgList(list)
end

return client
