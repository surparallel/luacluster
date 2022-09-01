--我们需要一个管理类，负责将entity工厂创建的entity注册到以剥离应用层的功能
local docker = require("docker")
local elog = require("eloghelp")
local int64 = require("int64")
local cmsgpack = require("cmsgpack")
local redishelp = require('redishelp')
---@type UdpProxy
local udpproxy = require 'udpproxy'
local sc = require("sc")
---@type  Entity
local entity = require("entity")

local entityMng = {}

function entityMng.PrintEntitys()
    for key, value in pairs(_G["__entity"]) do
        print(key,":", value.__obj)
    end

end

function entityMng.RegistryObj(obj)

    local id = docker.AllocateID()
    if id == nil or id == 0 then
        elog.sys_error("RegistryObj::AllocateID error")
        return nil
    end

    local myid,error = int64.new_unsigned(id)
    if(myid == nil)then
        elog.node_error("RegistryObj:: name:%s error:%s", obj.__class, error)
        return
    end

    --注册到全局对象
    if _G["__entity"] == nil then
        _G["__entity"] = {}
    end

    _G["__entity"][tostring(myid)] = obj
    obj.id = id

    return id
end

function entityMng.NewEntity(name, arg)

    local e = new(name)

    if e ~= nil then

        e:CopyArg(arg)
        entityMng.RegistryObj(e)
        local myint64,error = int64.new_unsigned(e.id)
        local myid = tostring(myint64)

        if(myint64 == nil)then
            elog.node_error("New::id name:%s error:%s", name, error)
            return
        end

        if(myid == '0')then
            elog.node_error("New::id is zero name:%s", name)
            return
        end

        elog.sys_details("New::id::%s(%s)",name, myid)

        if(e.clientid ~= nil)then
            entityMng.BindObj(e.id, e.clientid)
        else
            elog.sys_details("main::proto_rpc_create::not BindObj::%s", name)
        end

        if type(e.Init) == "function" then
            e:DoInheritFun(e, "Init")
        end

        --注册DNS
        if arg ~= nil and arg["DNS"] ~= nil then
            redishelp:hset("DNS",arg["DNS"], myid)
        end

        --注册一个负载均衡
        if arg ~= nil and arg["BALANCE"] ~= nil then
            redishelp:select(1)
            redishelp:lpush(arg["BALANCE"],myid)
            redishelp:select(0)
        end
    else
        elog.sys_error("main::proto_rpc_create:: New error::%s",name)
    end
end

function entityMng.UnRegistryObj(id)

    if _G["__entity"] == nil then
        return
    else
        local myid = int64.new_unsigned(id)
        local smyid = tostring(myid)
        if _G["__entity"][smyid] ~= nil then
            _G["__entity"][smyid] = nil
        else
            elog.sys_error("main::UnRegistryObj:: error::%s", smyid)
        end
    end
   
    docker.UnallocateID(id)
end

function entityMng.FindObj(id)

    local obj
    if _G["__entity"] == nil then
        return
    else
        local myid = int64.new_unsigned(id)
        obj = _G["__entity"][tostring(myid)]
    end

    return obj
end

function entityMng.BindObj(id, client)
    docker.BindNet(id, client)
end

--注册需要定时器更新的对象
function entityMng.RegistryUpdata(obj)
    if _G["__update"] == nil then
        _G["__update"] = {}
    end
    local myid = int64.new_unsigned(obj.id)
    _G["__update"][tostring(myid)] = obj
end

function entityMng.UnRegistryUpdata(obj)
    if _G["__update"] == nil then
        return
    end
    local myid = int64.new_unsigned(obj.id)
    _G["__update"][tostring(myid)] = nil
end

function entityMng.InitUpdata()
    _G["__updatelast"] = docker.GetTick()
    _G["__updatecount"] = 0
end

function entityMng.LoopUpdata(deltaTime, count)
    if _G["__update"] ~= nil then
        for k,v in pairs(_G["__update"]) do
            if v["Update"] ~= nil then
                v:Update(count, deltaTime)
            end
        end
    end
end

function entityMng.EntityToCreate(type, name, arg)
    docker.CreateEntity(type, cmsgpack.pack(name,arg))
end

function entityMng.RegistrySev(ServerName, obj)
    local myid = int64.new_unsigned(obj.id)
    redishelp:hset("servers",obj.ServerName,tostring(myid))
end

function entityMng.UnRegistrySev(ServerName)
    redishelp:hdel("servers",ServerName)
end

function entityMng.GetSev(ServerName)
    local id = redishelp:hget("servers", ServerName)
    if id ~= nil then
        return udpproxy(id)
    else
        elog.sys_error("entityMng.GetSev not find %s",ServerName)
        return nil
    end
end

function entityMng.EntityDataCreate(id, data)
    if _G["__EntityData"] == nil then
        _G["__EntityData"] = {}
    end

    if _G["__EntityDataCount"] == nil then
        _G["__EntityDataCount"] = {}
    end

    if _G["__EntityDataCount"][id] == nil then
        _G["__EntityDataCount"][id] = 0
    end
    
    _G["__EntityData"][id] = data
    _G["__EntityDataCount"][id] = _G["__EntityDataCount"][id] + 1
end

function entityMng.EntityDataSet(id, data)

    if _G["__EntityData"] == nil or _G["__EntityData"][id] == nil then
        entityMng.EntityDataCreate(id, data)
    end
    _G["__EntityData"][id] = data
end

function entityMng.EntityDataGet(id)

    if _G["__EntityData"] == nil or _G["__EntityData"][id] == nil then
        elog.sys_warn("entityMng.EntityDataGet after EntityDataCreate %s",id)
        return nil
    end
    return _G["__EntityData"][id]
end

function entityMng.EntityDataFree(id)

    if _G["__EntityDataCount"][id] ~= nil then
        _G["__EntityDataCount"][id] = _G["__EntityDataCount"][id] - 1

        if _G["__EntityDataCount"][id] <= 0 then
            _G["__EntityDataCount"][id] = nil
            _G["__EntityData"][id]  = nil
        end
    end
end

return entityMng