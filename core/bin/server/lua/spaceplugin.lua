--spaceplugin
local sc = require 'sc'
local entitymng = require("entitymng")
local docker = require("docker")
local int64 = require("int64")
local spaceproxy = require 'spaceproxy'
local udpproxy = require 'udpproxy'
local math3d = require 'math3d'
local elog = require("eloghelp")
local entity = require("entity")

local spacepluginFactory = {}

function spacepluginFactory.New()
    local obj = entity.New()
    obj.spaceInfo = {}
    obj.transform = {}
    obj.transform.poition = {}
    obj.transform.nowPotion = {}
    obj.transform.rotation = {}
    obj.entities = {}

    --这里要改为链接bigworld
    function obj:EntryWorld(sapceName)
        --从redis获取对象并调用空间的EntryWorld
        --entitymng.RegistryUpdata(self) 需要在子类的updata调用，引用self数据错误
        local space = entitymng.GetSev(sapceName)
        space:EntryWorld(self.id, self.transform.poition.x, self.transform.poition.z
            , self.transform.rotation.y
            , self.transform.velocity
            , self.transform.stamp
            , self.transform.stampStop
            , self.isGhost
        )
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

    --当entity在bigworld模式下获得空间时
    function obj:OnGetSpace(spaceType, beginx, beginz, endx, endz)

    end

    --用户主动离开
    function obj:LeaveWorld(sapceName)
        --从redis获取对象并调用空间的LeaveWorld
        for k, v in pairs(self.spaces) do
            v.LeaveWorld(self.id)
            self.spaces[k] = nil
        end
    end

    --可能会收到多个空间的进入可见列表
    function obj:OnAddView(entity)
        local id64 = int64.new_unsigned(entity[1])
        self.entities[tostring(id64)] = entity

        --转发到客户端
        if self.clientid ~= nil then
            docker.CopyRpcToClient()
        end
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
            local x,y,z =math3d.Position(v[2], 0, v[3], 0, v[4], 0, v[5], v[6])
            local dist = math3d.Dist(self.transform.poition.x, 0, self.transform.poition.z, x, y, z)
            if dist > limit then
                self.entities[k] = nil
            end
        end
    end

    --引导进入其他空间
    function obj:OnRedirectToSpace(spaces)

        for k, v in pairs(spaces) do

            local id64 = int64.new_unsigned(v)
            self.spaces[tostring(id64)] = udpproxy.New(id64.tonumber(id64))

            --首次登录
            self.spaces[tostring(id64)]:EntryWorld(self.id, self.transform.poition.x,
            self.transform.poition.z, self.transform.rotation.y,
            self.transform.velocity, self.transform.stamp,  self.transform.stampStop, self.isGhost)

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

    function obj:OnDelGhost(id)

        if self.master ~= nil then
            if self.master == id then
                return
            end
            local remoteCall = udpproxy.New(self.master)
            remoteCall:SetGhost(self.id)
        end

        self.master = id
    end

    function obj:EntityPosition(id)

        local id64 = int64.new_unsigned(id)
        if self.entities[tostring(id64)] == nil then
            return nil
        end

        local entity = self.entities[tostring(id64)]
        return math3d.Position(entity[2], 0, entity[3], 0, entity[4], 0, entity[5], entity[6], entity[7])
    end

    --其他客户端调用更改可见范围内的状态
    function obj:OnMove(id, poitionx, poitionz, rotationy, velocity, stamp, stampStop)

        local id64 = int64.new_unsigned(id)
        local sid = tostring(id64)
        if self.entities[sid] == nil then
            self.entities[sid] = {}
        end

        self.entities[sid][1] = id
        self.entities[sid][2] = poitionx
        self.entities[sid][3] = poitionz
        self.entities[sid][4] = rotationy
        self.entities[sid][5] = velocity
        self.entities[sid][6] = stamp
        self.entities[sid][7] = stampStop

        --转发到客户端
        if self.clientid ~= nil then
            docker.CopyRpcToClient()
        end
    end

    function obj:Move(poitionx, poitionz, rotationy, velocity, stamp)

        --通知空间
        for k, v in pairs(self.spaces) do
            v:Move(self.id, poitionx, poitionz, rotationy, velocity, stamp)
        end

        --通知客户端
        self.clients:OnMove(self.id, poitionx, poitionz, rotationy, velocity, stamp)
    end

    return obj
end

return spacepluginFactory