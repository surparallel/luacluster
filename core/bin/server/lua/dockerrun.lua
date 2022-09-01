local sc = require("sc")

local LuaPanda = nil
if sc.LuaPanda.close == 0 then
    LuaPanda = require("LuaPanda").start(sc.LuaPanda.ip,sc.LuaPanda.port)
end

local cmsgpack = require("cmsgpack")
local entitymng = require("entitymng")
local docker = require("docker")
local elog = require("eloghelp")
local try = require("try-catch-finally")
local cluster = require("cluster")
local int64 = require("int64")

function EntryUpdate(deltaTime, count)
    entitymng.LoopUpdata(deltaTime, count)
    if  _G["dockerID"] == 0 and not cluster:IsBooted() then
        --检查集群是否已经初始化，如果没有初始化就进入服务启动流程
        cluster:CheckInit()
    end
end

function EntryProcess(ret)
    if ret ~= nil and next(ret) == nil then
    elseif ret[1] == sc.proto.proto_ctr_cancel then
        return
    elseif ret[1] == sc.proto.proto_rpc_create then
        try(function()
            local arg = {cmsgpack.unpack(ret[2])}
            entitymng.NewEntity(arg[1], arg[2])
        end).catch(function (ex)
            elog.sys_error(ex)
        end)
    elseif ret[1] == sc.proto.proto_rpc_destory then
        try(function()
            --来自网络层通知网络断开销毁对应的对象
            local obj = entitymng.FindObj(ret[2])
            if obj ~= nil then
                --这里要再调用一次客户端确保消息队列清空
                if obj.client ~= nil then
                    obj.client:Destory()
                end

                --通知对象销毁
                obj:DoInheritFun(obj, "Destory")
            else
                local myid = tostring(int64.new_unsigned(ret[2]))
                elog.node_error("main::proto_rpc_destory:: not find obj: %s" ,myid)
            end
        end).catch(function (ex)
            elog.sys_error(ex)
        end)
    elseif ret[1] == sc.proto.proto_rpc_call then
        try(function()
            local myid = tostring(int64.new_unsigned(ret[2]))
            local obj = entitymng.FindObj(ret[2])
            if obj ~= nil then
                local arg = {cmsgpack.unpack(ret[3])}
                local funName = arg[1]
                if type(obj[funName]) == 'function' then
                    table.remove(arg, 1)
                    obj[funName](obj, table.unpack(arg))
                else
                    elog.node_error("main::proto_rpc_call:: not find function:%s(%s).%s",obj.__class, myid, funName)
                end
            else
                elog.node_error("main::proto_rpc_call:: not find obj:%s" ,myid)
            end
        end).catch(function (ex)
            elog.sys_error(ex)
        end)
    elseif ret[1] == sc.proto.proto_route_call then
        --注意这里机器人和服务器处理流程是不同的。
        try(function()
            local obj = entitymng.FindObj(ret[3])

            if obj ~= nil then
                if obj.isconnect then
                    --这里是机器人处理流程
                    if type(obj.entityCall) == 'function' then
                        obj:entityCall(ret[2], cmsgpack.unpack(ret[4]))
                    else
                        elog.node_error("main::proto_route_call:: not find fun entityCall")
                    end
                else
                    --这里是服务端处理流程
                    local arg = {cmsgpack.unpack(ret[4])}
                    local funName = arg[1]
                    if type(obj[funName]) == 'function' then
                        if obj:HaveKeyFlags(sc.keyflags.exposed, funName) then
                            table.remove(arg, 1)
                            obj[funName](obj, table.unpack(arg))
                        else
                            elog.node_error("main::proto_route_call:: not exposed fun:%s", funName)
                        end
                    else
                        elog.node_error("main::proto_route_call:: not find fun: %s", funName)
                    end
                end
            else
                elog.node_error("main::proto_route_call:: not find obj: %u",ret[3])
            end

        end).catch(function (ex)
            elog.sys_error(ex)
        end)
    elseif ret[1] == sc.proto.proto_ctr_list then
        entitymng.PrintEntitys()
    else
        elog.node_error("main:: not proto")
    end
end

function main()
    entitymng.InitUpdata()
    docker.Loop()
end
