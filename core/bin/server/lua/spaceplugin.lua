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
        space:EntryWorld(self.id, self.transform.poition.x, self.transform.poition.y, self.transform.poition.z
            , self.transform.rotation.x
            , self.transform.rotation.y
            , self.transform.rotation.z
            , self.transform.velocity
            , self.transform.stamp
            , self.transform.stampStop
            , self.isGhost
        )
        self.clients = spaceproxy.New(self)
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
        local id64 = tostring(int64.new_unsigned(entity[1]))

        if self.entities[id64] == nil then
            entitymng:EntityDataCreate(id64, entity)
        else
            entitymng:EntityDataSet(id64, entity)
        end

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

        for k, v1 in pairs(self.entities) do
            local v = entitymng.EntityDataGet(k)
            local x,y,z =math3d.Position(v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10])
            local dist = math3d.Dist(self.transform.poition.x, 0, self.transform.poition.z, x, y, z)
            if dist > limit then
                self.entities[k] = nil
                entitymng.EntityDataFree(k)
            end
        end
    end

    --引导进入其他空间
    function obj:OnRedirectToSpace(spaces)

        if #spaces == 0 then
            elog.sys_error("spaceplugin::OnRedirectToSpace:: spaces is empty")
            return
        end

        --1，种情况master为空，在spaces中选一个。
        --2，master不为空，但不在spaces中，删除从新设置一个。
        --3，master不为空，在spaces中，什么都不做。
        local isGhost = 0
        local isFind = 0

        for k, v in pairs(spaces) do
            if self.master ~= nil and self.master == v then
                isFind = 1
            end
        end

        if self.master ~= nil and isFind ~= 1 then
            --重置主空间
            local myid = int64.new_unsigned(self.id)
            local id64 = int64.new_unsigned(self.master)
            elog.sys_error("spaceplugin::OnRedirectToSpace::lost master id:%s spaceid:%s count:%i ", tostring(myid), tostring(id64), #spaces)
            local remoteCall = udpproxy.New(self.master)
            remoteCall:SetGhost(self.id)
            self.master = nil
            isGhost = 0
        elseif self.master ~= nil  and isFind == 1 then
            isGhost = 1
        end

        local tspaces = self.spaces
        self.spaces = {}

        for k, v in pairs(spaces) do
            local id64 = int64.new_unsigned(v)
            local key = tostring(id64)

            if(tspaces[key] ~= nil) then
                self.spaces[key] = tspaces[key]

                if isGhost == 0 then
                    self.spaces[key]:EntryWorld(self.id, self.transform.poition.x,
                    self.transform.poition.y,
                    self.transform.poition.z,
                    self.transform.rotation.x,
                    self.transform.rotation.y,
                    self.transform.rotation.z,
                    self.transform.velocity, self.transform.stamp,  self.transform.stampStop, isGhost)
                    self.master = v
                    isGhost = 1
                end
            else
                self.spaces[key] = udpproxy.New(id64.tonumber(id64))
                --首次登录
                self.spaces[key]:EntryWorld(self.id, self.transform.poition.x,
                self.transform.poition.y,
                self.transform.poition.z,
                self.transform.rotation.x,
                self.transform.rotation.y,
                self.transform.rotation.z,
                self.transform.velocity, self.transform.stamp,  self.transform.stampStop, isGhost)

                if isGhost == 0 then
                    self.master = v

                    local myid = int64.new_unsigned(self.id)
                    elog.sys_details("spaceplugin::OnRedirectToSpace::master exchang myid:%s master:%s", tostring(myid), key)
                    isGhost = 1
                end
            end
        end

         --master为空，并且空间都是旧有的，从新指定空间
        if self.master == nil then
            local id64 = int64.new_unsigned(spaces[1])
            local key = tostring(id64)
            local proxy = self.spaces[key]
            proxy:EntryWorld(self.id, self.transform.poition.x,
            self.transform.poition.y,
            self.transform.poition.z,
            self.transform.rotation.x,
            self.transform.rotation.y,
            self.transform.rotation.z,
            self.transform.velocity, self.transform.stamp,  self.transform.stampStop, 0)

            self.master = spaces[1]

            local myid = int64.new_unsigned(self.id)
            elog.sys_error("spaceplugin::OnRedirectToSpace::lost myid:%s master:%s", tostring(myid), key)
        end
    end

    --空间管理器通知离开
    function obj:OnLeaveSpace(id)
        local id64 = int64.new_unsigned(id)
        self.spaces[tostring(id64)] = nil
    end

    function obj:OnDelGhost(id)

        local tspaces = self.spaces
        self.spaces = {}
        local isempty = 0;

        local id64 = int64.new_unsigned(id)
        local key = tostring(id64)

        for k, v in pairs(tspaces) do
            if(k == key) then
                self.spaces[key] = v
                isempty = 1
            end
        end

        if isempty == 0 then
            elog.sys_error("spaceplugin::OnDelGhost::lost spaces %s", key)
        end

        if self.master ~= nil then
            if self.master == id then
                return
            end
            local remoteCall = udpproxy.New(self.master)
            remoteCall:SetGhost(self.id)
        end

        local myid = int64.new_unsigned(self.id)
        elog.sys_details("spaceplugin::OnDelGhost id:%s master:%s", tostring(myid), key)
        self.master = id
    end

    function obj:EntityPosition(id)

        local id64 = int64.new_unsigned(id)
        if self.entities[tostring(id64)] == nil then
            return nil
        end

        local entity = self.entities[tostring(id64)]
        return math3d.Position(entity[2], entity[3], entity[4], entity[5], entity[6], entity[7], entity[8], entity[9], entity[10])
    end

    --其他客户端调用更改可见范围内的状态
    function obj:OnMove(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)

        local id64 = int64.new_unsigned(id)
        local sid = tostring(id64)

        entitymng.EntityDataSet(sid, {id, poitionx, poitiony, poitionz, rotationx
        , rotationy, rotationz, velocity, stamp, stampStop})

        --转发到客户端
        if self.clientid ~= nil then
            docker.CopyRpcToClient()
        end
    end

    function obj:Move(poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)

        --通知空间
        for k, v in pairs(self.spaces) do
            v:Move(self.id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
        end

        --通知客户端这里可以使用转发省去了序列化
        self.clients:OnMove(self.id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
    end

    return obj
end

return spacepluginFactory