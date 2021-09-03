--space_sudoku
local sc = require 'sc'
local entity = require("entity")
local entitymng = require("entitymng")
local cmsgpack = require("cmsgpack")
local docker = require("docker")
local int64 = require("int64")
local elog = require("elog")
local sudokuapi = require("sudokuapi")

local spaceFactory= {}

function spaceFactory.New(arg)
    print("New sudoku")
    local obj = entity.New(arg)
    obj.entities = {}
    
    --注册自己的entity id到redis
    function obj:Init()
        entitymng.RegistrySev(self.ServerName, self)
        entitymng.RegistryUpdata(self)
        obj.mysudoku = sudokuapi.Create(sc.sukoku.girdx
                                        , sc.sukoku.girdz
                                        , sc.sukoku.beginx
                                        , sc.sukoku.beginz
                                        , sc.sukoku.endx
                                        , sc.sukoku.endz)
    end
    
    function obj:update(count, deltaTime)
        sudokuapi.Update(obj.mysudoku)
    end

    function obj:EntryWorld(id, poitionx, poitionz, rotationy, velocity, stamp)
        
        local entryid = int64.new_unsigned(id)
        sudokuapi.Entry(obj.mysudoku, entryid, poitionx, poitionz, rotationy, velocity, stamp)
    end

    function obj:LeaveWorld(id)
        --从redis获取对象并调用空间的LeaveWorld
        sudokuapi.Leave(obj.mysudoku, id)
    end
    
    function obj:Destory()
        entitymng.UnRegistryUpdata(self)
        sudokuapi.Destroy(obj.mysudoku)
    end

    return obj
end

return spaceFactory