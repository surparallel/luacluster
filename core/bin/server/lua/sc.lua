--system config
local dcopy = require "dcopy"
local try = require("try-catch-finally")
local elog = require("eloghelp")
local sc = {}

sc.proto = {}
sc.proto.proto_rpc_create = 1
sc.proto.proto_rpc_call = 2
sc.proto.proto_rpc_destory = 3
sc.proto.proto_route_call = 4
sc.proto.proto_ctr_cancel = 5
sc.proto.proto_run_lua = 6
sc.proto.proto_net_bind = 7
sc.proto.proto_net_destory = 8
sc.proto.proto_net_connect = 9
sc.proto.proto_client_id = 10
sc.proto.proto_client_entity = 11

sc.entity = {}
sc.entity.DockerCurrent = 0 --当前ddocker
sc.entity.DockerRandom = 1 --当前节点的随机ddocker
sc.entity.NodeInside = 2 --任意内部节点
sc.entity.NodeOutside = 3 --任意有对外部通信节点
sc.entity.NodeRandom = 4
sc.entity.DockerGlobe = 5 --放入全局节点

--属性的标识符
sc.keyflags = {}
sc.keyflags.exposed = 1--客户端可调用
sc.keyflags.persistent = 2--保存到数据库
sc.keyflags.broadcast = 4--广播给所有可见对象
sc.keyflags.private = 8--同步到客户端

-----------以上为常量，以下为配置----------------------------------------------------------------------------------------------------
sc.glob = {}
sc.glob.mysc = "assert_sc"--客户资产的配置文件名，如果存在将使用客户资产的配置文件覆盖当前文件配置

sc.LuaPanda = {}
sc.LuaPanda.ip = "127.0.0.1"
sc.LuaPanda.port = 8818
sc.LuaPanda.close = 0

sc.redis = {}
sc.redis.ip = "127.0.0.1"
sc.redis.port = 6379

sc.cluster = {}
sc.cluster.serves = {"space","bigworld","dbsvr"}--集群启动时要启动的服务列表
sc.cluster.expire = 15 --集群关键key的过期时间秒
sc.cluster.nodeSize = 1 --集群的数量
sc.cluster.nodeStar = 5 --集群服务的启动延迟时间

sc.sudoku = {}
sc.sudoku.girdx = 100
sc.sudoku.girdz = 100
sc.sudoku.beginx = 0
sc.sudoku.beginz = 0
sc.sudoku.endx = 100
sc.sudoku.endz = 100
sc.sudoku.radius = 15
sc.sudoku.outsideSec = 10 * 60 * 1000

sc.bigworld = {}
sc.bigworld.beginx = 0
sc.bigworld.beginz = 0
sc.bigworld.endx = 100
sc.bigworld.endz = 100

sc.db = {}
sc.db.uri= 'mongodb://127.0.0.1'
sc.db.dbname = 'luacluster-mongo'
sc.db.collname = 'entity'
sc.db.updateTime = 300

try(function()

    if sc.glob.mysc==nil then
        return
    end

    local mysc = require(sc.glob.mysc)
    if mysc ~= nil then
        dcopy.clone(sc, mysc)
    end

end).catch(function (ex)
    elog.sys_warn(ex)
end)

return sc