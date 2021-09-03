local entity = require("entity")
local test = {}

function test.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")
end

function test.New(arg)
    print("New")
    local obj = entity.New(arg)

    function obj:fun()
        print("test entity fun")
    end

    function obj:fun2(a, b, c)
        print("test entity fun2")
    end

    return obj
end

return test