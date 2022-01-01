local elog = require("eloghelp")
local cmsgpack = require("cmsgpack")
local docker = require("docker")

local udpProxyFactory = {}

function udpProxyFactory.New(id)

    if id == 0 or id == nil then
        error("udpProxyFactory id == 0")
    end

    local obj = {}
    obj.id = id
    obj.__entity = 1
    return setmetatable(obj,{
        __index = function (t,k)
            return function(...)
                -- 只有变成table之后才能使用table的函数进行插入或删除甚至修改
                -- 打包的时候就已经是脱壳的了
                local arg={...}
                local self = arg[1]
                if self ~= obj then
                    elog.sys_error("udpProxyFactory Object must be accessed by \": \")!")
                    return
                end
                arg[1] = k
                docker.Send(self.id, cmsgpack.pack(table.unpack(arg)))
            end
        end,
        __newindex = function (t,k,v)
            elog.sys_error("updProxy does not accept setting data!")
            return
        end
    })
end

return udpProxyFactory