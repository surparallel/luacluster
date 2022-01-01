local entity = require("entity")
local tcpproxy = require("tcpproxy")
local tabtostr = require 'tabtostr'
local int64 = require("int64")
local entitymng = require("entitymng")

local botsFactory = {}

function botsFactory.New()
    local obj = entity.New()
    obj.a = 1

    function obj:Init()
        self.server = tcpproxy.New(self.id)
        entitymng.RegistryUpdata(self)
    end

    function obj:Destory()
    end

    function obj:Update(count, deltaTime)
        obj.server:ping(obj.a)
    end

    function obj:pong(a)
        obj.a = a + 1
        print("bot pong::", a)
    end

    function obj:entityCall(id, fun, ...)
        --print("bot fun::", tostring(int64.new_unsigned(id)), fun)

        if fun == 'pong' then
            local arg = {...}
            obj.a = arg[1] + 1
            --print("bot pong::", obj.a)
        end
    end
    return obj
end

return botsFactory