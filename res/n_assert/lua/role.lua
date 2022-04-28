local entity = require("entity")
local tcpproxy = require("tcpproxy")
local sc = require("sc")
local docker = require("docker")
local int64 = require("int64")
local elog = require("eloghelp")
local entitymng = require("entitymng")
local udpproxylist = require 'udpproxylist'
local cmsgpack = require("cmsgpack")

local roleFactory = {}

function roleFactory.New()
    local obj = entity.New()
    obj:Inherit("client")

    function obj:Init()
        local myid = tostring(int64.new_unsigned(self.id))
        elog.sys_fun("role::init %s", myid)
    end

    return obj
end

return roleFactory