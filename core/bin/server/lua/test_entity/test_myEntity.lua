package.path = package.path .. ";./test_entity/?.lua"
local json = require("dkjson")
local msgpack = require "msgpack"
require("entity")

---@type MyEntity
local obj = new("myEntity")
local obj2 = new("myEntity")
obj2.cc.a = 2

local a = 1
obj.__inherit.parant.fun1(obj)--这时候使用self要强转成子类
obj:fun1()
obj.c = {}


