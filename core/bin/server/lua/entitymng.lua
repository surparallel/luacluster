--我们需要一个管理类，负责将entity工厂创建的entity注册到以剥离应用层的功能
local docker = require("docker")
local elog = require("elog")
local int64 = require("int64")
local cmsgpack = require("cmsgpack")
local redishelp = require('redishelp')
local udpproxy = require 'udpproxy'

local entityMng = {}

_G["__updatelast"] = docker.GetCurrentMilli()
_G["__updatecount"] = 0

function entityMng.RegistryObj(obj)

    local id = docker.AllocateID()
    if id == nil then
        return nil
    end

    --注册到全局对象
    if _G["__entity"] == nil then
        _G["__entity"] = {}
    end

    local myid = int64.new_unsigned(id)
    _G["__entity"][tostring(myid)] = obj
    obj.id = id
    
    return id
end

function entityMng.CreateEntity(name, arg)
    
    local e = require(name).New(arg)
    if e ~= nil then
        entityMng.RegistryObj(e)
        
        local myid = int64.new_unsigned(e.id)
        elog.details("New::"..name..",id::"..tostring(myid))

        if(e.clientid ~= nil)then
            entityMng.BindObj(e.id, e.clientid)
        else
            elog.details("main::proto_rpc_create::not BindObj::",name)
        end

        if type(e.Init) == "function" then
            e:Init()
        end
    else
        elog.error("main::proto_rpc_create:: New error::",name)
    end
end

function entityMng.UnRegistryObj(id)

    if _G["__entity"] == nil then
        return
    else
        local myid = int64.new_unsigned(id)
        _G["__entity"][tostring(myid)] = nil
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

function entityMng.LoopUpdata()

    if _G["__update"] == nil then
        return
    end

    local last = _G["__updatelast"]
    local count = _G["__updatecount"]
    local now = docker.GetCurrentMilli()
    local deltaTime = now - last
    count = count + 1
    
    for k,v in pairs(_G["__update"]) do
        if v.update ~= nil then
            v:update(count, deltaTime)
        end
    end

    _G["__updatelast"] = now
    _G["__updatecount"] = count
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
        return udpproxy.New(id)
    end
    return nil
end

return entityMng