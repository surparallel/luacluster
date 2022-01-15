--bigworld
local sc = require 'sc'
local entity = require("entity")
local entitymng = require("entitymng")
local elog = require("eloghelp")
local bigworldapi = require("bigworldapi")
local udpproxy = require 'udpproxy'
local docker = require("docker")
--local mri = require("MemoryReferenceInfo")

local spaceFactory= {}

function spaceFactory.New()
    local obj = entity.New()
    obj.entities = {}
    obj.split = 0;--测试分裂空间
    
    --注册自己的entity id到redis
    function obj:Init()
        
        elog.sys_fun("bigworld::init %i", _G["bots"])
        --在客户端模式下不运行
        if(_G["bots"] == 1) then
            return
        end

        entitymng.RegistrySev(self.ServerName, self)
        self.apihandle = bigworldapi.Create(sc.sudoku.girdx
                         , sc.sudoku.girdz
                         , sc.bigworld.beginx
                         , sc.bigworld.beginz
                         , sc.bigworld.endx
                         , sc.bigworld.endz
                        )
        --创建基本的space
        entitymng.EntityToCreate(sc.entity.DockerZero , "sudokuex", {bigworld=self.id,
                                                                    beginx = sc.bigworld.beginx - sc.sudoku.girdx,
                                                                    beginz = sc.bigworld.beginz - sc.sudoku.girdz,
                                                                    endx = sc.bigworld.endx + sc.sudoku.girdx,
                                                                    endz = sc.bigworld.endz + sc.sudoku.girdz,
                                                                    oid = 0 })
        self.girdx = sc.sudoku.girdx
        self.girdz = sc.sudoku.girdz
        self.beginx = sc.bigworld.beginx
        self.beginz = sc.bigworld.beginz
        self.endx = sc.bigworld.endx
        self.endz = sc.bigworld.endz
        self.spaceType = "bigworld"

        self.mycount = 1
     
        entitymng.RegistryUpdata(self)
--[[         require "profiler"
        profiler = newProfiler("call", 1)
        profiler:start()
       mri.m_cMethods.DumpMemorySnapshot("./", "1-Before", -1)]]
    end

    function obj:Update(count, deltaTime)

--[[          print(count,"\n")

        if count > 300 then

            print("************profile stop************")
            profiler:stop()

            local outfile = io.open( "profile.txt", "w+"  )
            profiler:report( outfile  )
            outfile:close()
          mri.m_cMethods.DumpMemorySnapshot("./", "2-After", -1)
            os.exit()
        end
        if self.mycount <= 10 then
            print(self.mycount,"\n")
            if self.mycount == 10 then
                for i = 1, 10, 1 do
                    entitymng.EntityToCreate(sc.entity.DockerRandom , "npc")
                end
                --打印每个线程的创建npc数量
                docker.Script("", 0, -1, "local elog = require(\"eloghelp\") elog.node_details(\"Analysis of entity allocation dockerid:%i, \
                 dockercount:%i\", dockerID, docker.GetEntityCount())");
            end
            self.mycount = self.mycount + 1
        end]]
    end
    
    --引导entity进入space 如果是多个就进入多个
    function obj:EntryWorld(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
        bigworldapi.Entry(self.apihandle, id, poitionx, poitionz)

        local entityProxy = udpproxy.New(id)
        entityProxy:OnEntryWorld(self.spaceType, self.beginx, self.beginz, self.endx, self.endz)
    end
    
    --理论上这个对象不会被销毁
    function obj:Destory()
        bigworldapi.Destroy()
    end

    function obj:SpaceFull(id)
--[[
        --检查空间是否已经在调整中，如果没有就开始调整根据返回创建两个新的空间数据
        local adjust, beginx, beginz, endx, endz = bigworldapi.SpaceFull(self.apihandle, id)
        if adjust == 1 then
            entitymng.EntityToCreate(sc.entity.DockerZero , "sudokuex", {bigworld=self.id,
                                                                        beginx = beginx,
                                                                        beginz = beginz,
                                                                        endx = endx,
                                                                        endz = endz,
                                                                        oid = id
                                                                        })
        end]]
    end

    --space创建成功，这里可能导致重复创建
    --SpaceFull创建成功要调整旧地图边界
    --新空间数据，原来空间数据，被调整空间数据
    --Init创建成功不做任何调整
    function obj:OnSpace(id, oid, beginx, beginz, endx, endz)
        --放入rtree 删除原空间，调整原空间后重新插入
        bigworldapi.OnSpace(self.apihandle, id, oid, beginx, beginz, endx, endz)

        --空间创建成功后测试分裂空间
        if self.split == 0 then
            self:SpaceFull(id)
            self.split = 1
        end
    end

    return obj
end

return spaceFactory