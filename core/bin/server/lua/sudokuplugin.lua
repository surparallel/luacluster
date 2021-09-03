--space_sudoku_plugin
local sc = require 'sc'
local entitymng = require("entitymng")
local cmsgpack = require("cmsgpack")
local docker = require("docker")
local int64 = require("int64")
local spaceproxy = require 'spaceproxy'

local spacePluginFactory = {}

function spacePluginFactory.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")

    if t:HaveKeyFlags(k, sc.keyflags.broadcast) then
        for k, v in pairs(t.entities) do
            local keyid = int64.new_unsigned(k)
            docker.SendToClient(t.id, keyid, cmsgpack.pack("ChangStatus", {[k] = v}))
        end
    end
end

function spacePluginFactory.New()
    local obj = {}

    function obj:EntryWorld(sapceName)
        --从redis获取对象并调用空间的EntryWorld
        local space = entitymng.GetSev(sapceName)
        space:EntryWorld(self.id, self.transform.poition.x, self.transform.poition.z
            , self.transform.rotation.y
            , self.transform.velocity
            , self.transform.stamp)
        self.clients = spaceproxy.New(self.id, space)
    end
 
    function obj:LeaveWorld(sapceName)
        --从redis获取对象并调用空间的LeaveWorld
        local space = entitymng.GetSev(sapceName)
        space:LeaveWorld(self.id)
        self.clients = nil
    end

    function obj:OnAddView(entityList)

        local entry = cmsgpack.unpack(entityList)
        for key, value in ipairs(entry) do
            -- key = entityid
            obj.entities[value[1]] = value
        end

        --转发到客户端
        docker.CopyRpcToClient()
    end

    function obj:OnMove(id, poitionx, poitionz, rotationy, velocity, stamp)

        if (obj.entities[id] ~= nil and stamp ~=0) then
            obj.entities[id][1] = id
            obj.entities[id][2] = poitionx
            obj.entities[id][3] = poitionz
            obj.entities[id][4] = rotationy
            obj.entities[id][5] = velocity
            obj.entities[id][6] = stamp
        elseif (obj.entities[id] ~= nil and stamp ==0) then
            obj.entities[id] = nil
        end

        --转发到客户端
        docker.CopyRpcToClient()
    end

    function obj:OnDelView(entityList)

    end

    return obj
end

return spacePluginFactory