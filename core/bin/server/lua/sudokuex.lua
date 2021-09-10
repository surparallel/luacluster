--space_sudoku
local sc = require 'sc'
local entity = require("entity")
local entitymng = require("entitymng")
local cmsgpack = require("cmsgpack")
local docker = require("docker")
local int64 = require("int64")
local elog = require("elog")
local sudokuapi = require("sudokuapi")
local udpproxy = require 'udpproxy'

local spaceFactory= {}

function spaceFactory.New(arg)
    local obj = entity.New(arg)
    obj.entities = {}
    
    --注册自己的entity id到redis
    function obj:Init()
        entitymng.RegistryUpdata(self)
        obj.mysudoku = sudokuapi.Create(sc.sukoku.girdx
                                        , sc.sukoku.girdz
                                        , self.beginx
                                        , self.beginz
                                        , self.endx
                                        , self.endz)

        local bigworld = udpproxy.new(self.bigworld)
        bigworld:OnSpace(self.id, self.beginx, self.beginz, self.endx, self.endz)

    end
    
    function obj:update(count, deltaTime)
        sudokuapi.Update(self.mysudoku)
    end

    function obj:EntryWorld(id, poitionx, poitionz, rotationy, velocity, stamp)
        sudokuapi.Entry(self.mysudoku, id, poitionx, poitionz, rotationy, velocity, stamp)
    end

    --todolist 移动导致格子和预测不同，要重新计算可见范围
    function obj:Move(id, poitionx, poitionz, rotationy, velocity, stamp)
        sudokuapi.Move(self.mysudoku, id, poitionx, poitionz, rotationy, velocity, stamp)
    end

    function obj:LeaveWorld(id)
        --从redis获取对象并调用空间的LeaveWorld
        sudokuapi.Leave(self.mysudoku, id)
    end
    
    function obj:Destory()
        entitymng.UnRegistryUpdata(self)
        sudokuapi.Destroy(self.mysudoku)
    end

    function obj:OnAlterSpace(msg)
        local unmsg = cmsgpack.unpakc(msg)

        sudokuapi.Alter(self.mysudoku, unmsg[2][1], unmsg[2][2], unmsg[2][3], unmsg[2][4], unmsg[2][5])
        sudokuapi.Insert(self.mysudoku, unmsg[1][1], unmsg[1][2], unmsg[1][3], unmsg[1][4], unmsg[1][5])
    end

    function obj:OnInitSpace(listspace)
        for i, v in ipairs(listspace) do
            sudokuapi.Insert(self.mysudoku, v[1], v[2], v[3], v[4], v[5])
        end
    end

    return obj
end

return spaceFactory