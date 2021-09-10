--space_sudoku
local sc = require 'sc'
local entity = require("entity")
local entitymng = require("entitymng")
local docker = require("docker")
local int64 = require("int64")
local elog = require("elog")
local bigworldapi = require("bigworldapi")
local udpproxy = require 'udpproxy'

local spaceFactory= {}

function spaceFactory.New(arg)
    local obj = entity.New(arg)
    obj.entities = {}
    
    --注册自己的entity id到redis
    function obj:Init()
        entitymng.RegistrySev(self.ServerName, self)
        self.apihandle = bigworldapi.Create(sc.sukoku.girdx
                         , sc.sukoku.girdz
                         , sc.bigworld.beginx
                         , sc.bigworld.beginz
                         , sc.bigworld.endx
                         , sc.bigworld.endz
                        )
        --创建基本的space
        entitymng.EntityToCreate(sc.entity.NodeInside , "sudokuex", {bigworld=self.id,
                                                                    beginx = sc.bigworld.beginx,
                                                                    beginz = sc.bigworld.beginz,
                                                                    endx = sc.bigworld.endx,
                                                                    endz = sc.bigworld.endz,
                                                                    oid = 0
                                                                    })
    end
    
    --引导entity进入space 如果是多个就进入多个
    function obj:EntryWorld(id, poitionx, poitionz)
        bigworldapi.Entry(self.apihandle, id, poitionx, poitionz)
    end
    
    function obj:Destory()
        bigworldapi.Destroy()
    end

    function obj:OnFull(id)

        --检查空间是否已经在调整中，如果没有就开始调整根据返回创建两个新的空间数据
        local adjust, beginx, beginz, endx, endz = bigworldapi.OnFull(id)
        if adjust == 1 then
            entitymng.EntityToCreate(self.apihandle, sc.entity.NodeInside , "sudokuex", {bigworld=self.id,
                                                                        beginx = beginx,
                                                                        beginz = beginz,
                                                                        endx = endx,
                                                                        endz = endz,
                                                                        oid = id
                                                                        })
        end

    end

    --space创建成功，这里可能导致重复创建
    --OnFull创建成功要调整旧地图边界
    --新空间数据，原来空间数据，被调整空间数据
    --Init创建成功不做任何调整
    function obj:OnSpace(id, oid, beginx, beginz, endx, endz)
        --放入rtree 删除原空间，调整原空间后重新插入
        bigworldapi.OnSpace(self.apihandle, id, oid, beginx, beginz, endx, endz)
    end

    return obj
end

return spaceFactory