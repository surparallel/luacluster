---@class Parant2
local parant2 = class()

function parant2.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")
end

function parant2:fun1() 
    print("parant2.fun1",self)
end

function parant2:Init()
    print("parant2 init", self)
end

return parant2