# luacluster

[![C/C++ CI](https://github.com/surparallel/luacluter/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/surparallel/luacluter/actions/workflows/c-cpp.yml)

## 概要

luacluster分布式游戏服务器框架。特色是实现了万人同屏，实现了无缝大地图，用户开发方式为lua的rpc调用模式，已经接入mongodb。近期计划是写技术白皮书。QQ群：927073440。

Lucluster distributed game server framework. The feature is to achieve the same screen for ten thousand people and a big seamless world. The user development mode is the rpc call mode of lua. Mongodb is already available. The immediate plan is to write a technical white paper.

## 1. BUILDING AND INSTALLATION

### CMake (Windows)

Install CMake: <https://www.cmake.org>

``` cmd
$ md build && cd build
$ cmake -G "Visual Studio 10" ..   # Or use any generator you want to use. Run cmake --help for a list
$ cmake --build . --config Release # Or "start libevent.sln" and build with menu in Visual Studio.
```

See [Documentation/Building#Building on Windows](/Documentation/Building.md#building-on-windows) for more information

### CMake (Ubuntu)

``` cmd
$ cd core && mkdir build
$ cd core/build && cmake ..     # Default to ubuntu Makefiles.
$ cd core && cmake --build build
```

### CMake  (optional)

``` cmd
$ cmake -DLUA_USE_LUAJIT=ON #使用luajit
```

## 2. START RUNING (Windows)

### 服务器

启动流程：redis.bat, luacluter(_d).exe, bots.bat

在bots.bat中修改要启动的机器人数量

### 机器人

``` cmd
# --btcp的参数为连接服务器的 Ip port 总数量
$ luacluter_d.exe --bots --noudp --inside --btcp 127.0.0.1 9577 500 #服务器ip, 端口，总数量

# --btcp2的参数为连接服务器的 Ip port 数量 ip数量
$ luacluter_d.exe --bots --noudp --inside --btcp2 127.0.0.1 9577 500 #服务器ip, 端口，数量， ip的数量
```

### 两种模式的配置

服务器的默认配置文件core\res\server\config_defaults.json

#### 压力测试模式

``` cmd
{
  "log_config": {
    "ruleDefault": {

    }
  },
  "base_config": {
    "inside": 1
  },
  "docke_config":{
    "dockerRandomSize":128,#随机线程的数量
    "dockerGlobeSize":8,#全局线程的数量
    "popNodeSize":200000,
    "congestion":4000000
  },
  "tcp":{
    "tcp4":[
      {
      "tcp4BindAddr":"0.0.0.0",#tcp的线程和ip端口
      "tcp4Port":9577
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9578
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9579
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9580
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9581
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9582
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9583
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9584
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9585
      }, 
      {
      "tcp4BindAddr":"0.0.0.0",
      "tcp4Port":9586
      }
    ]
  }
}
```

#### 单线程调试模式

``` cmd
{
  "log_config": {
    "ruleDefault": {

    }
  },
  "base_config": {
    "inside": 1
  },
  "docke_config":{
    "dockerRandomSize":1,#随机线程的数量
    "dockerGlobeSize":0,#全局线程的数量
    "popNodeSize":200000,
    "congestion":4000000
  },
  "tcp":{
    "tcp4":[
      {
      "tcp4BindAddr":"0.0.0.0",#tcp的线程和ip端口
      "tcp4Port":9577
      }
    ]
  }
}
```

### 数据库存储的测试

在luacluster中使用mongodb

sc.lua全局配置中已经添加了"dbsvr"全局服务对象。

``` lua
sc.cluster.serves = {"space","bigworld","dbsvr"}--集群启动时要启动的服务列表
```

在任意节点控制台输入命令创建一个dbentity对象

``` cmd
#命令new
#参数1：在什么地方创建对象
	DockerCurrent = 0, //当前线程的docker
	DockerRandom,//当前节点的随机ddocker
	NodeInside,//任意内部节点
	NodeOutside,//任意有对外部通信节点
	NodeRandom,//随机节点
	DockerGlobe//放入全局对象池
#参数2：创建对象的名字“dbentity”
#参数3：创建对象的初始化数据格式为json
>new 1 dbentity {}
>New tentity:   18302840717262782469
```

使用call命令调用dbentity对象的fun函数返回dbid。

``` cmd
>call 18302840717262782469 fun
>test entity fun
db      6253f48671099513a0f5165f

#命令call
#参数1：entityid
#参数2：entity的函数,这里为dbentity.lua中的“fun”
#剩余参数:为函数参数,类型为数字，字符串，json。例如call 18302840717262782469 fun2 111 {"a"：1} aaa
```

## 3. luacluster白皮书纲要

1. [luacluster组织架构及名词定义](./docs/1.整体架构设计.md)
2. [entity对象的创建，继承，多重继承，多态，健事件。](./docs/2.entity的面向对象.md)
3. luarpc调用及封包结构。
4. 全局对象的创建和全局对象的功能插件
5. bigworld无缝大地图全局对象。
6. sudoku九宫格空间全局对象。
7. space全局对象
8. dbsvr数据存储全局对象
9. 命令以及参数
10. docker的脚本api
11. 如何使用luacluster创建一个MMO游戏
