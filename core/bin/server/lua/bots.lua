local entity = require("entity")
local tcpproxy = require("tcpproxy")
local tabtostr = require 'tabtostr'

local botsFactory = {}

function botsFactory.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")
end

function botsFactory.New()
    local obj = entity.New()

    function obj:Init()
        self.server = tcpproxy.New(self.id)
    end

    function obj:Destory()
    end

    function obj:pong(a)
        a = a + 1
        print("bot pong::", a)
        obj.server:ping(a)
    end

    function obj:entityCall(id, fun, ...)
        print("bot fun::", id, fun)

        if fun == 'pong' then
            local arg = {...}
            local a = arg[1] + 1
            print("bot pong::", a)
            obj.server:ping(a)
        end
    end
    return obj
end

return botsFactory