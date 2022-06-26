--space_sudoku
local sc = require 'sc'
local entity = require("entity")
local entitymng = require("entitymng")
local docker = require("docker")
local int64 = require("int64")
local elog = require("eloghelp")
local sudokuapi = require("sudokuapi")
local udpproxy = require 'udpproxy'

local spaceFactory= {}

function spaceFactory.New()
    local obj = entity.New()
    obj.entities = {}

    --注册自己的entity id到redis
    function obj:Init()

        if(_G["bots"] == 1) then
            return
        end
        
        entitymng.RegistrySev(self.ServerName, self)
        entitymng.RegistryUpdata(self)
        self.mysudoku = sudokuapi.Create(sc.sudoku.girdx
                                        , sc.sudoku.girdz
                                        , sc.sudoku.beginx
                                        , sc.sudoku.beginz
                                        , sc.sudoku.endx
                                        , sc.sudoku.endz
                                        , 0
                                        , self.id
                                        , sc.sudoku.outsideSec)
        self.girdx = sc.sudoku.girdx
        self.girdz = sc.sudoku.girdz
        self.beginx = sc.sudoku.beginx
        self.beginz = sc.sudoku.beginz
        self.endx = sc.sudoku.endx
        self.endz = sc.sudoku.endz
        self.spaceType = "sudoku"
    end

    function obj:Update(count, deltaTime)
        sudokuapi.Update(self.mysudoku)
    end

    function obj:EntryWorld(id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost)
        sudokuapi.Entry(self.mysudoku, id, poitionx, poitiony, poitionz, rotationx, rotationy, rotationz, velocity, stamp, stampStop, isGhost, 0)

        local entityProxy = udpproxy.New(id)
        entityProxy:OnEntryWorld(self.spaceType, self.beginx, self.beginz, self.endx, self.endz)

        local entityProxy = udpproxy.New(id)
        entityProxy:OnGetSpace(self.id)
    end

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

    return obj
end

return spaceFactory