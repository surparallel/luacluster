local elog = require("eloghelp")
local cmsgpack = require("cmsgpack")
local docker = require("docker")

---@class TcpProxy
local tcpProxyFactory = {}
function tcpProxyFactory.New(did, sid)
    local obj = {}
    obj.did = did
    if sid == nil then
        obj.sid = did
    else
        obj.sid = sid
    end
    --当前是一个entity代理对象不需要对属性进行监控
    obj.__entity = 1
    function obj:PID(pid)
        self.pid = pid
        return self
    end

    return setmetatable(obj,{
        __index = function (t,k)
            return function(...)
                -- 只有变成table之后才能使用table的函数进行插入或删除甚至修改
                -- 打包的时候就已经是脱壳的了
                local arg={...}
                local self = arg[1]
                if self ~= obj then
                    elog.sys_error("tcpProxyFactory Object must be accessed by \": \")!")
                    return
                end

                arg[1] = k
                --npc 试图控制可见范围内客户端对象状态时
                --这个接口会直接通过网络层转发到客户端
                docker.SendToClient(self.did, self.sid, cmsgpack.pack(table.unpack(arg)))
            end
        end,
        __newindex = function (t,k,v)
            elog.sys_error("tcpProxy does not accept setting data!")
            return
        end
    })
end

setmetatable(tcpProxyFactory, { __call = function(_, ...) return tcpProxyFactory.New(...) end })
return tcpProxyFactory