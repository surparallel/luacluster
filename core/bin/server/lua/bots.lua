local entity = require("entity")
local tcpproxy = require("tcpproxy")
local tabtostr = require 'tabtostr'
local int64 = require("int64")

local botsFactory = {}

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
        print("bot fun::", tostring(int64.new_unsigned(id)), fun)

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