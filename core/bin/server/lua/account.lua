local entity = require("entity")
local tcpproxy = require("tcpproxy")
local sc = require("sc")
local docker = require("docker")
local int64 = require("int64")

local accountFactory = {}

function accountFactory.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")

    if t:HaveKeyFlags(k, sc.keyflags.private) then
        docker.SendToClient(t.id, t.id, cmsgpack.pack("ChangStatus", {[k]=v}))
    end
end

function accountFactory.New()
    local obj = entity.New()
    obj:AddKeyFlags("ping", sc.keyflags.exposed)
    obj:Inherit("moveplugin")
    obj.client = tcpproxy.New(obj.id)

    obj.transform = {}
    obj.transform.poition = {}
    obj.transform.poition.x = 1
    obj.transform.poition.y = 1
    obj.transform.poition.z = 1

    obj.transform.rotation = {}
    obj.transform.rotation.x = 0
    obj.transform.rotation.z = 0
    obj.transform.rotation.y = 0

    obj.transform.velocity = 0
    obj.transform.stamp = os.time()
    obj.transform.stampStop = 0

    function obj:Init()

        elog.fun("account::init")
        if(obj.clientid ~= nil)then
            obj.client = tcpproxy.New(self.id)
        end

        obj:EntryWorld("bigworld")
    end

    function obj:Destory()
        obj:LeaveWorld("bigworld")
    end
    
    function obj:ping(a)
        a = a + 1
        print("account ping::", a)
        obj.client:pong(a)
    end

    return obj
end

return accountFactory