--spaceplugin
local sc = require 'sc'
local entitymng = require("entitymng")
local docker = require("docker")
local int64 = require("int64")
local spaceproxy = require 'spaceproxy'
local sudokuapi = require("sudokuapi")
local udpproxy = require 'udpproxy'

local spacepluginFactory = {}

function spacepluginFactory.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")

    if t:HaveKeyFlags(k, sc.keyflags.broadcast) then
        for k, v in pairs(t.entities) do
            local keyid = int64.new_unsigned(k)
            docker.SendToClient(t.id, keyid, cmsgpack.pack("ChangStatus", {[k] = v}))
        end
    end
end

function spacepluginFactory.New()
    local obj = {}
    self.spaceInfo = {}

    --这里要改为链接bigworld
    function obj:EntryWorld(sapceName)
        --从redis获取对象并调用空间的EntryWorld
        --entitymng.RegistryUpdata(self) 需要在子类的updata调用，引用self数据错误
        local space = entitymng.GetSev(sapceName)
        space:EntryWorld(self.id, self.transform.poition.x, self.transform.poition.z
            , self.transform.rotation.y
            , self.transform.velocity
            , self.transform.stamp
            , self.isGhost
            , self.transform.stampStop)
        self.clients = spaceproxy.New(self)
        self.isGhost = 0
        self.spaces = {}
    end

    function obj:OnEntryWorld(spaceType, beginx, beginz, endx, endz)
        self.spaceInfo.spaceType = spaceType
        self.spaceInfo.beginx = beginx
        self.spaceInfo.beginz = beginz
        self.spaceInfo.endx = endx
        self.spaceInfo.endz = endz

    end

    --用户主动离开
    function obj:LeaveWorld(sapceName)
        --从redis获取对象并调用空间的LeaveWorld
        for k, v in pairs(self.spaces) do
            v.LeaveWorld(self.id)
            spaces[k] = nil
        end
    end

    --可能会收到多个空间的进入可见列表
    function obj:OnAddView(entityList)
        for key, value in ipairs(entityList) do
            -- key = entityid
            self.entities[key] = value
        end

        --转发到客户端
        docker.CopyRpcToClient()
    end

    --其他客户端调用更改可见范围内的状态
    function obj:OnMove(id, poitionx, poitionz, rotationy, velocity, stamp, stampStop)

        if (self.entities[id] ~= nil and stamp ~=0) then
            self.entities[id][1] = id
            self.entities[id][2] = poitionx
            self.entities[id][3] = poitionz
            self.entities[id][4] = rotationy
            self.entities[id][5] = velocity
            self.entities[id][6] = stamp
            self.entities[id][7] = stampStop
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
    function obj:Update(count, deltaTime)

        local limit = math.sqrt(sc.sudoku.girdx * sc.sudoku.girdx + sc.sudoku.girdz * sc.sudoku.girdz)
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

        for k, v in pairs(spaces) do

            local id64 = int64.new_unsigned(v)
            self.spaces[tostring(id64)] = udpproxy.New(id64.tonumber(id64))

            --首次登录
            self.spaces[tostring(id64)]:EntryWorld(self.id, self.transform.poition.x,
            self.transform.poition.z, self.transform.rotation.y,
            self.transform.velocity, self.transform.stamp,  self.isGhost, self.transform.stampStop)

            if self.isGhost == 0 then
                self.master = v
            end

            self.isGhost = 1
        end
    end

    --空间管理器通知离开
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

    function obj:OnDelGhost(id)

        if self.master.id == id then
            return
        end

        if self.master ~= nil then
            local remoteCall = udpproxy.New(self.master)
            remoteCall:SetGhost(self.id)
        end

        self.master = id
    end

    return obj
end

return spacepluginFactory