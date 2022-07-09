---@class Parant3
local parant3 = class()

function parant3.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")
end

function parant3:fun1() 
    print("parant2.fun1",self)
end

function parant3:Init()
    print("parant3 init", self)
end

return parant3