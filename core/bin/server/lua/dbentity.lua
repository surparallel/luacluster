local entity = require("entity")
local int64 = require("int64")
local sc = require("sc")
local json = require("dkjson")
local test = {}

function test.New()
    local obj = entity.New()
    obj:Inherit("dbplugin")
    obj:AddKeyFlags("a", sc.keyflags.persistent)
    obj:AddKeyFlags("b", sc.keyflags.persistent)
    obj:AddKeyFlags("c", sc.keyflags.persistent)
    obj.stamp = os.time()

    function obj:Init()
        local myid = tostring(int64.new_unsigned(self.id))
        print("New dbentity:",myid)
    end

    function obj:fun()
        print("dbentity fun")
        self.a = 1
        self.b = {a = 1, b = 2}
        self.c = {1,2,3,4}
        table.remove(self.c, 1)
        self:Save()
    end

    function  obj:SaveBack(dbid)
        self.__allparant["dbplugin"].SaveBack(self,dbid)
        print("dbentity id",dbid)
        self.b = {a = 3, b = 4}
        self:SaveUpdate()
        self:Load(dbid)
    end

    function  obj:LoadBack(data)
        self.__allparant["dbplugin"].LoadBack(self, data)
    end

    function obj:fun2(a, b, c)
        print("dbentity fun2:",a," ",b,"",c)
    end

    function obj:mongo()
        require(assert(("test"), 'test script is missing'))
    end

    function obj:Update(count, deltaTime)
        --定期保存
       if self.stamp - os.time() > sc.db.updateTime then
        self:SaveUpdate()
       end
    end

    return obj
end

return test