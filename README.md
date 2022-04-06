# luacluster 
[![C/C++ CI](https://github.com/surparallel/luacluter/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/surparallel/luacluter/actions/workflows/c-cpp.yml)

### 概要
luacluster分布式游戏服务器框架。特色是实现了万人同屏，实现了无缝大地图，用户开发方式为lua的rpc调用模式。近期开发计划是接入mongodb。QQ群：927073440

# 1. BUILDING AND INSTALLATION
## CMake (Windows)

Install CMake: <https://www.cmake.org>

```
$ md build && cd build
$ cmake -G "Visual Studio 10" ..   # Or use any generator you want to use. Run cmake --help for a list
$ cmake --build . --config Release # Or "start libevent.sln" and build with menu in Visual Studio.
```

See [Documentation/Building#Building on Windows](/Documentation/Building.md#building-on-windows) for more information

## CMake (Ubuntu)

```
$ cd core && mkdir build
$ cd core/build && cmake ..     # Default to ubuntu Makefiles.
$ cd core && cmake --build build
```

## CMake  (optional)

```
$ cmake -DLUA_USE_LUAJIT=ON #使用luajit
```

# 2. START RUNING (Windows)
## 服务器

启动流程：redis.bat, luacluter(_d).exe, bots.bat

在bots.bat中修改要启动的机器人数量

## 机器人
```
# --btcp的参数为连接服务器的 Ip port 总数量
$ luacluter_d.exe --bots --noudp --inside --btcp 127.0.0.1 9577 500 #服务器ip, 端口，总数量

# --btcp2的参数为连接服务器的 Ip port 数量 ip数量
$ luacluter_d.exe --bots --noudp --inside --btcp2 127.0.0.1 9577 500 #服务器ip, 端口，数量， ip的数量
```
## 两种模式的配置
服务器的默认配置文件core\res\server\config_defaults.json

**压力测试模式**
``` 
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

**单线程调试模式**
``` 
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
