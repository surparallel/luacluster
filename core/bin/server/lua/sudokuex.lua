--space_sudoku
local sc = require 'sc'
local entity = require("entity")
local entitymng = require("entitymng")
local elog = require("eloghelp")
local sudokuapi = require("sudokuapi")
local udpproxy = require 'udpproxy'

local spaceFactory= {}

function spaceFactory.New()
    local obj = entity.New()
    obj.entities = {}
    
    --注册自己的entity id到redis
    function obj:Init()

        elog.sys_fun("sudokuex::init")
        entitymng.RegistryUpdata(self)

        if self.bigworld == nil then
            self.mysudoku = sudokuapi.Create(sc.sudoku.girdx
                                            , sc.sudoku.girdz
                                            , self.beginx
                                            , self.beginz
                                            , self.endx
                                            , self.endz
                                            , 0
                                            , self.id
                                            , sc.sudoku.outsideSec)
        else
            self.mysudoku = sudokuapi.Create(sc.sudoku.girdx
                                            , sc.sudoku.girdz
                                            , self.beginx
                                            , self.beginz
                                            , self.endx
                                            , self.endz
                                            , 1
                                            , self.id
                                            , sc.sudoku.outsideSec)

            local bigworld = udpproxy.New(self.bigworld)
            bigworld:OnSpace(self.id, self.oid, self.beginx, self.beginz, self.endx, self.endz)
        end

        self.spaceType = "sudokuex"
    end

    function obj:Update(count, deltaTime)
        sudokuapi.Update(self.mysudoku, count, deltaTime)
    end

    function obj:EntryWorld(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost)
        sudokuapi.Entry(self.mysudoku, id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost)

        local entityProxy = udpproxy.New(id)
        entityProxy:OnGetSpace(self.id)
    end

    --todolist 移动导致格子和预测不同，要重新计算可见范围
    function obj:Move(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
        sudokuapi.Move(self.mysudoku, id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop)
    end

    function obj:LeaveWorld(id)
        --从redis获取对象并调用空间的LeaveWorld
        sudokuapi.Leave(self.mysudoku, id)
    end
    
    function obj:Destory()
        entitymng.UnRegistryUpdata(self)
        sudokuapi.Destroy(self.mysudoku)
    end

    function obj:OnAlterSpace(new, alter)
        if alter ~= nil then
            sudokuapi.Alter(self.mysudoku, alter[1], alter[2], alter[3], alter[4], alter[5])
        end
        sudokuapi.Insert(self.mysudoku, new[1], new[2], new[3], new[4], new[5])
    end

    function obj:OnInitSpace(listspace)
        for i, v in ipairs(listspace) do
            sudokuapi.Insert(self.mysudoku, v[1], v[2], v[3], v[4], v[5])
        end
    end

    function obj:SetGhost(id)
        sudokuapi.SetGhost(self.mysudoku, id)
    end

    return obj
end

return spaceFactory