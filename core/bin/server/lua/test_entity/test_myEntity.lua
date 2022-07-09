package.path = package.path .. ";./test_entity/?.lua"
local json = require("dkjson")
local msgpack = require "msgpack"
require("entity")

---@type MyEntity
local obj = new("myEntity")

local a = 1
obj.__inherit.parant.fun1(obj)--这时候使用self要强转成父类
obj:fun1()
obj.c = {}

local obj2 = new()
obj.z = {}


