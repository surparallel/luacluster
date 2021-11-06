--spaceproxy
local elog = require("eloghelp")
local cmsgpack = require("cmsgpack")
local docker = require("docker")
local int64 = require("int64")

local spaceProxyFactory = {}

function spaceProxyFactory.New(entity)
    local obj = {}
    obj.id = entity.id
    obj.entity = entity
    return setmetatable(obj,{
        __index = function (t,k)
            return function(...)
                -- 只有变成table之后才能使用table的函数进行插入或删除甚至修改
                -- 打包的时候就已经是脱壳的了
                local arg={...}
                local self = arg[1]
                if self ~= obj then
                    elog.error("spaceProxy Object must be accessed by \": \")!")
                    return
                end
                
                arg[1] = k
                for k, v in pairs(obj.entity.entities) do
                    local keyid = int64.new_unsigned(k)
                    docker.SendToClient(t.id, keyid, cmsgpack.pack(arg))
                end
            end
        end,
        __newindex = function (t,k,v)
            elog.error("spaceProxy does not accept setting data!")
            return
        end
    })
end

return spaceProxyFactory