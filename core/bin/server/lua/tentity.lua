local entity = require("entity")
local test = {}

function test.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")
end

function test.New()
    print("New")
    local obj = entity.New()

    function obj:fun()
        print("test entity fun")
    end

    function obj:fun2(a, b, c)
        print("test entity fun2")
    end

    function obj:mongo()
        require(assert(("test"), 'test script is missing'))
    end

    return obj
end

return test