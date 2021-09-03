local sc = require("sc")
local redishelp = require("redishelp")
local entitymng = require("entitymng")
local elog = require("elog")
local cluster = {}

function cluster:CheckInit()

    if true == redishelp:setnx('cluster:init',"programmer") then
        redishelp:expire('cluster:init', sc.cluster.expire)

        for k, v in pairs(sc.cluster.serves) do
            entitymng.EntityToCreate(sc.entity.DockerRandom, v, {ServerName=k})
        end
    else
        elog.details("cluster:CheckInit() Win a silver medal")
    end
    return
end

return cluster