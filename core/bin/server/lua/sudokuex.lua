--space_sudoku
local sc = require 'sc'
---@type Entity
local entity = require("entity")
local entitymng = require("entitymng")
local elog = require("eloghelp")
local sudokuapi = require("sudokuapi")
---@type UdpProxy
local udpproxy = require 'udpproxy'

---@class Sudokuex
local sudokuex = class()

sudokuex.entities = {}

--注册自己的entity id到redis
function sudokuex:Init()

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

        local bigworld = udpproxy(self.bigworld)
        bigworld:OnSpace(self.id, self.oid, self.beginx, self.beginz, self.endx, self.endz)
    end

    self.spaceType = "sudokuex"
end

function sudokuex:Update(count, deltaTime)
    sudokuapi.Update(self.mysudoku, count, deltaTime)
end

function sudokuex:EntryWorld(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost)
    sudokuapi.Entry(self.mysudoku, id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost, 0)

    local entityProxy = udpproxy(id)
    entityProxy:OnGetSpace(self.id)
end

--todolist 移动导致格子和预测不同，要重新计算可见范围
function sudokuex:Move(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost)
    sudokuapi.Move(self.mysudoku, id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost, 0)
end

function sudokuex:LeaveWorld(id)
    --从redis获取对象并调用空间的LeaveWorld
    sudokuapi.Leave(self.mysudoku, id)
end

function sudokuex:Destory()
    entitymng.UnRegistryUpdata(self)
    sudokuapi.Destroy(self.mysudoku)
end

function sudokuex:OnAlterSpace(new, alter)
    if alter ~= nil then
        sudokuapi.Alter(self.mysudoku, alter[1], alter[2], alter[3], alter[4], alter[5])
    end
    sudokuapi.Insert(self.mysudoku, new[1], new[2], new[3], new[4], new[5])
end

function sudokuex:OnInitSpace(listspace)
    for i, v in ipairs(listspace) do
        sudokuapi.Insert(self.mysudoku, v[1], v[2], v[3], v[4], v[5])
    end
end

function sudokuex:SetGhost(id)
    sudokuapi.SetGhost(self.mysudoku, id)
end

return sudokuex