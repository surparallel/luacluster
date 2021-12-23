local sc = require("sc")
local redishelp = require("redishelp")
local entitymng = require("entitymng")
local elog = require("eloghelp")
local cluster = {}

cluster.isBoots = 0

function cluster:CheckInit()

    if self.isBoots == 0 then
        --todo 这里要记录获得了权限,并且数量和时间要保证
        if true == redishelp:setnx('cluster:init',"doing") then
            redishelp:expire('cluster:init', sc.cluster.expire)
            self.starTime = os.time()
            self.isBoots = 1
        else
            self.isBoots = 2
            elog.details("cluster:CheckInit() Win a silver medal")
        end
    elseif self.isBoots == 1 and (sc.cluster.nodeSize <= redishelp:scard('cluster:nodeall') or sc.cluster.nodeStar <= (os.time() - self.starTime)) then

        for k, v in pairs(sc.cluster.serves) do
            entitymng.EntityToCreate(sc.entity.DockerZero, v, {ServerName=v})
        end

        redishelp:set('cluster:init',"done")
        redishelp:expire('cluster:init', sc.cluster.expire)
        self.isBoots = 2
    end

    return
end

function cluster:IsBooted()
    if self.isBoots == 2 then
        return true
    else 
        return false
    end
end

function cluster:IsDone()
    if redishelp:set('cluster:init') == "done" then
        return true
    else
        return false
    end
end

return cluster