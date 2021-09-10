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

function accountFactory.New(arg)
    local obj = entity.New(arg)
    obj:AddKeyFlags("ping", sc.keyflags.exposed)
    obj:Inherit("spaceplugin")
    obj.client = tcpproxy.New(obj.id)

    function obj:Init()
        if(obj.clientid ~= nil)then
            obj.client = tcpproxy.New(self.id)
        end

        obj:EntryWorld("space")
    end

    function obj:Destory()
        obj:LeaveWorld("space")
    end
    
    function obj:ping(a)
        a = a + 1
        print("account ping::", a)
        obj.client:pong(a)
    end

    return obj
end

return accountFactory