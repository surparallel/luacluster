--space_sudoku_plugin
local sc = require 'sc'
local entitymng = require("entitymng")
local docker = require("docker")
local int64 = require("int64")
local spaceproxy = require 'spaceproxy'
local sudokuapi = require("sudokuapi")
local udpproxy = require 'udpproxy'

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

    --这里要改为链接bigworld
    function obj:EntryWorld(sapceName)
        --从redis获取对象并调用空间的EntryWorld
        --entitymng.RegistryUpdata(self) 需要在子类的updata调用，引用self数据错误
        local bigworld = entitymng.GetSev(sapceName)
        bigworld:EntryWorld(self.id, self.transform.poition.x, self.transform.poition.z
            , self.transform.rotation.y
            , self.transform.velocity
            , self.transform.stamp)
        self.clients = spaceproxy.New(self)
        self.isGhost = 0
        self.spaces = {}
    end

    --可能离开多个空间
    function obj:LeaveWorld(sapceName)
        --从redis获取对象并调用空间的LeaveWorld
        local space = entitymng.GetSev(sapceName)
        space:LeaveWorld(self.id)
        self.clients = nil
    end

    --可能会收到多个空间的可见列表
    function obj:OnAddView(entityList)
        for key, value in ipairs(entityList) do
            -- key = entityid
            self.entities[value[1]] = value
        end

        --转发到客户端
        docker.CopyRpcToClient()
    end

    --其他客户端调用更改可见范围内的状态
    function obj:OnMove(id, poitionx, poitionz, rotationy, velocity, stamp)

        if (self.entities[id] ~= nil and stamp ~=0) then
            self.entities[id][1] = id
            self.entities[id][2] = poitionx
            self.entities[id][3] = poitionz
            self.entities[id][4] = rotationy
            self.entities[id][5] = velocity
            self.entities[id][6] = stamp
        elseif (self.entities[id] ~= nil and stamp == 0) then
            self.entities[id] = nil
        end

        --转发到客户端
        docker.CopyRpcToClient()
    end

    --废弃
    function obj:OnDelView(entityList)

    end

    function obj:Destory()
        entitymng.UnRegistryUpdata(self)
    end
    -- 删除不可见的对象
    function obj:update(count, deltaTime)

        local limit = math.sqrt(sc.sukoku.girdx * sc.sukoku.girdx + sc.sukoku.girdz * sc.sukoku.girdx)
        limit = limit * 2;

        for k, v in pairs(self.entities) do
            local dist = sudokuapi.Dist(self.transform.poition.x, self.transform.poition.z, v[2], v[3], v[4], v[5], v[6])
            if dist > limit then
                self.entities[k] = nil
            end
        end
    end

    --引导进入空间可能会有多个，要分出ghost
    function obj:OnEntrySpace(spaces)
        for i, v in ipairs(spaces) do
            local space = udpproxy.New(v)

            local id64 = int64.new_unsigned(space)
            self.spaces[id64] = udpproxy.New(id64)
            --首次登录
            space:EntryWorld(self.id, self.transform.poition.x, self.transform.poition.z, self.rotation.y, self.transform.velocit, self.transform.stamp,  self.isGhost)
            self.isGhost = 1
        end
    end

    --self.spaces用于move的广播通知空间
    function obj:OnLeaveSpace(id)
        local id64 = int64.new_unsigned(id)
        self.spaces[tostring(id64)] = nil
    end

    function obj:Move(poitionx, poitionz, rotationy, velocity, stamp)
        local id64 = int64.new_unsigned(self.id)
        self.spaces[tostring(id64)] = nil
        for k, v in pairs(self.spaces) do

            v:Move(self.id, poitionx, poitionz, rotationy, velocity, stamp)
        end

        self.clients:OnMove(self.id, poitionx, poitionz, rotationy, velocity, stamp)
    end

    return obj
end

return spacePluginFactory