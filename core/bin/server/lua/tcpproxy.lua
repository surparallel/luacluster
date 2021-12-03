local elog = require("eloghelp")
local cmsgpack = require("cmsgpack")
local docker = require("docker")

local tcpProxyFactory = {}

function tcpProxyFactory.New(id)
    local obj = {}
    obj.did = id
    obj.sid = id
    obj.__entity = 1
    function obj:PID(pid)
        obj.pid = pid
        return obj
    end

    return setmetatable(obj,{
        __index = function (t,k)
            return function(...)
                -- 只有变成table之后才能使用table的函数进行插入或删除甚至修改
                -- 打包的时候就已经是脱壳的了
                local arg={...}
                local self = arg[1]
                if self ~= obj then
                    elog.error("tcpProxyFactory Object must be accessed by \": \")!")
                    return
                end
                
                arg[1] = k
                docker.SendToClient(obj.did, obj.sid, cmsgpack.pack(table.unpack(arg)))
            end
        end,
        __newindex = function (t,k,v)
            elog.error("tcpProxy does not accept setting data!")
            return
        end
    })
end

return tcpProxyFactory