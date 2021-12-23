local sc = require("sc")
local LuaPanda = require("LuaPanda").start(sc.LuaPanda.ip,sc.LuaPanda.port)
local cmsgpack = require("cmsgpack")
local entitymng = require("entitymng")
local docker = require("docker")
local elog = require("eloghelp")
local try = require("try-catch-finally")
local cluster = require("cluster")
local int64 = require("int64")

function main()
    
    entitymng.InitUpdata()
    local waittime = sc.glob.msec
    while(true)
    do
        if  _G["dockerid"] == 0 and not cluster:IsBooted() then
            --检查集群是否已经初始化，如果没有初始化就进入服务启动流程
            cluster:CheckInit()
        end

        local stamp = docker.GetCurrentMilli()
        local ret = {docker.Wait(waittime)}

        --补偿时间
        if #ret ~= 0 then
            waittime = waittime - stamp
            if waittime < 0 then
                waittime = 0
            end
        end

        if #ret == 0 then
            waittime = sc.glob.msec - entitymng.LoopUpdata()
            if waittime < 0 then
                waittime = 0
            end
        elseif ret[1] == sc.proto.proto_ctr_cancel then
            return
        elseif ret[1] == sc.proto.proto_rpc_create then
            try(function()
                local arg = {cmsgpack.unpack(ret[2])}
                entitymng.NewEntity(arg[1], arg[2])
            end).catch(function (ex)
                elog.error(ex)
            end)
        elseif ret[1] == sc.proto.proto_rpc_destory then
            try(function()
                --来自网络层通知网络断开销毁对应的对象
                local obj = entitymng.FindObj(ret[2])
                if obj ~= nil then

                    --通知对象销毁
                    obj:DoInheritFun(obj, "Destory")

                    --管理器销毁
                    entitymng.UnRegistryObj(ret[2])
                else
                    local myid = tostring(int64.new_unsigned(ret[2]))
                    elog.error("main::proto_rpc_destory:: not find obj: %s" ,myid)
                end
            end).catch(function (ex)
                elog.error(ex)
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
                        elog.error("main::proto_rpc_call:: not find function:%s(%s).%s",obj.__class, myid, funName)
                    end
                else
                    elog.error("main::proto_rpc_call:: not find obj:%s" ,myid)
                end
            end).catch(function (ex)
                elog.error(ex)
            end)
        elseif ret[1] == sc.proto.proto_route_call then
            --注意这里客户端和服务器处理流程是不同的。
            --因为服务器的npcic在客户端是不能创建同id的entity对象
            --所以在客户端这个协议是调用主entity对象的一个映射方法
            --这个判断是依据所调用的主entity是否为客户端对象做出的
            --在服务器则只能调用当前主entity的方法，不能调用其他对象的方法
            --对于需要转发到客户端的rpc协议，可以使用一个获取指令和语法糖来解决
            try(function()
                local obj = entitymng.FindObj(ret[3])
                if obj.isconnect then
                    --服务器发送给客户端
                    if obj ~= nil then
                        if type(obj.entityCall) == 'function' then
                            obj:entityCall(ret[2], cmsgpack.unpack(ret[4]))
                        else
                            elog.error("main::proto_route_call:: not find fun entityCall")
                        end
                    end
                else
                    --客户端发送给服务器端
                    obj = entitymng.FindObj(ret[2])
                    if obj ~= nil then
                        local arg = {cmsgpack.unpack(ret[4])}
                        local funName = arg[1]
                        if type(obj[funName]) == 'function' then
                            if obj:HaveKeyFlags(funName, sc.keyflags.exposed) then
                                table.remove(arg, 1)
                                obj[funName](obj, table.unpack(arg))
                            else
                                elog.error("main::proto_route_call:: not exposed fun:%s", funName)
                            end
                        else
                            elog.error("main::proto_route_call:: not find fun: %s", funName)
                        end
                    else
                        elog.error("main::proto_route_call:: not find obj: %u",ret[2])
                    end
                end

            end).catch(function (ex)
                elog.error(ex)
            end)
        else
            elog.error("main:: not proto")
        end
    end
end
