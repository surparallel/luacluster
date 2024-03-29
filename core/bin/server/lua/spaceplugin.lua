--spaceplugin
local sc = require 'sc'
local entitymng = require("entitymng")
local docker = require("docker")
local int64 = require("int64")
---@type SpaceProxy
local spaceproxy = require 'spaceproxy'
---@type UdpProxy
local udpproxy = require 'udpproxy'
local math3d = require 'math3d'
local elog = require("eloghelp")
---@type Entity
local entity = require("entity")

---@class SpacePlugin
local spacePlugin = class()

spacePlugin.spaceInfo = {}
spacePlugin.transform = {}
spacePlugin.transform.position = {}
spacePlugin.transform.nowPosition = {}
spacePlugin.transform.rotation = {}
spacePlugin.transform.stamp = 0 
spacePlugin.entities = {}
spacePlugin.spaces = {}

--这里要改为链接bigworld
function spacePlugin:EntryWorld(sapceName)
    
    --从redis获取对象并调用空间的EntryWorld
    --entitymng.RegistryUpdata(self) 需要在子类的updata调用，引用self数据错误
    local space = entitymng.GetSev(sapceName)

    space:EntryWorld(self.id, self.transform.position.x, self.transform.position.y, self.transform.position.z
        , self.transform.rotation.x
        , self.transform.rotation.y
        , self.transform.rotation.z
        , self.transform.velocity
        , self.transform.stamp
        , self.transform.stampStop
        , self.isGhost)
    self.clients = spaceproxy(self)
end

function spacePlugin:OnEntryWorld(spaceType, beginx, beginz, endx, endz)
    self.spaceInfo.spaceType = spaceType
    self.spaceInfo.beginx = beginx
    self.spaceInfo.beginz = beginz
    self.spaceInfo.endx = endx
    self.spaceInfo.endz = endz
end

--当entity在bigworld模式下获得空间时
function spacePlugin:OnGetSpace(spaceType, beginx, beginz, endx, endz)

end

--用户主动离开所有空间
function spacePlugin:LeaveWorld(sapceName)

    if self.spaces ~= nil and next(self.spaces) == nil  then
        elog.sys_error("spaceplugin::LeaveWorld:: self.spaces is empty")
        return
    end

    for k, v in pairs(self.spaces) do
        v:LeaveWorld(self.id)
        self.spaces[k] = nil
    end
    --通知视野内对象删除可见
    for k, v1 in pairs(self.entities) do
        local v = entitymng.EntityDataGet(k)
        if v ~= nil then
            local view = udpproxy(v[1])
            view:OnDelView(self.id);
        end
    end
end

--可能会收到多个空间的进入可见列表
--在初始化过程中可能会导致客户端无法收到
function spacePlugin:OnAddView(entity)
    local id64 = tostring(int64.new_unsigned(entity[1]))

    self.entities[id64] = 1
    entitymng.EntityDataSet(id64, entity)

    --转发到客户端
    if self.clientid ~= nil then
        docker.CopyRpcToClient(entity[1])
    end

    --如果stamp和当前的不一致要补发当前最新的状态
    if entity[12] ~= self.transform.stamp and self.transform.stamp > entity[12] then
        local view = udpproxy(entity[1])
        view:OnMove(self.id, self.transform.position.x, self.transform.position.y, self.transform.position.z
        ,self.transform.rotation.x, self.transform.rotation.y, self.transform.rotation.z, self.transform.velocity
        , self.transform.stamp, self.transform.stampStop)
    end
end

function spacePlugin:OnDelView(id)
    local k = tostring(int64.new_unsigned(id))
    self.entities[k] = nil
    entitymng.EntityDataFree(k)

    --转发到客户端
    if self.clientid ~= nil then
        docker.CopyRpcToClient(id)
    end
end

function spacePlugin:Destory()
    entitymng.UnRegistryUpdata(self)
end

-- 删除不可见的对象
function spacePlugin:Update(count, deltaTime)

    local limit = math.sqrt(sc.sudoku.girdx * sc.sudoku.girdx + sc.sudoku.girdz * sc.sudoku.girdz)
    limit = limit * 2;

    for k, v1 in pairs(self.entities) do
        local v = entitymng.EntityDataGet(k)
        if v ~= nil then
            local x,y,z =math3d.Position(v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10])
            local dist = math3d.Dist(self.transform.position.x, 0, self.transform.position.z, x, y, z)
            if dist > limit then
                self.entities[k] = nil
                entitymng.EntityDataFree(k)
            end
        end
    end
end

--引导进入其他空间可能有多个空间
function spacePlugin:OnRedirectToSpace(spaces)

    if next(spaces) == nil  then
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
        local remoteCall = udpproxy(self.master)
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
                self.spaces[key]:EntryWorld(self.id, self.transform.position.x,
                self.transform.position.y,
                self.transform.position.z,
                self.transform.rotation.x,
                self.transform.rotation.y,
                self.transform.rotation.z,
                self.transform.velocity, self.transform.stamp,  self.transform.stampStop, isGhost)
                self.master = v
                isGhost = 1
            end
        else
            self.spaces[key] = udpproxy(id64.tonumber(id64))
            --首次登录
            self.spaces[key]:EntryWorld(self.id, self.transform.position.x,
            self.transform.position.y,
            self.transform.position.z,
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
        proxy:EntryWorld(self.id, self.transform.position.x,
        self.transform.position.y,
        self.transform.position.z,
        self.transform.rotation.x,
        self.transform.rotation.y,
        self.transform.rotation.z,
        self.transform.velocity, self.transform.stamp,  self.transform.stampStop, 0)

        self.master = spaces[1]

        local myid = int64.new_unsigned(self.id)
        elog.sys_error("spaceplugin::OnRedirectToSpace::lost myid:%s master:%s", tostring(myid), key)
    end
end

--在多个空间切换时空间管理器通知离开
function spacePlugin:OnLeaveSpace(id)
    local id64 = int64.new_unsigned(id)
    self.spaces[tostring(id64)] = nil
end

function spacePlugin:OnDelGhost(id)

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
        local remoteCall = udpproxy(self.master)
        remoteCall:SetGhost(self.id)
    end

    local myid = int64.new_unsigned(self.id)
    elog.sys_details("spaceplugin::OnDelGhost id:%s master:%s", tostring(myid), key)
    self.master = id
end

function spacePlugin:EntityPosition(id)

    local id64 = int64.new_unsigned(id)
    if self.entities[tostring(id64)] == nil then
        return nil
    end

    local entity = self.entities[tostring(id64)]
    return math3d.Position(entity[2], entity[3], entity[4], entity[5], entity[6], entity[7], entity[8], entity[9], entity[10])
end

--其他客户端调用更改可见范围内的状态
function spacePlugin:OnMove(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)

    local id64 = int64.new_unsigned(id)
    local sid = tostring(id64)

    entitymng.EntityDataSet(sid, {id, poitionx, poitiony, poitionz, rotationx
    , rotationy, rotationz, velocity, stamp, stampStop})

    --转发到客户端
    if self.clientid ~= nil then
        docker.CopyRpcToClient(id)
    end
end

function spacePlugin:SpaceMove(poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)

    --通知空间
    local isGhost = 1
    for k, v in pairs(self.spaces) do
        if self.master == v then
            isGhost = 0
        else
            isGhost = 1
        end
        v:Move(self.id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost)
    end
end

return spacePlugin