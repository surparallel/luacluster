---@class Parant
local parant = class("parant3")

parant.cc = {}

function parant.OnFreshKey(t,k,v,o,f)
    print("OnFreshKey")
end

function parant:fun1() 
    print("parant.fun1",self)
end

---@param a Table
function parant:Init(a)
    print("myEntity init", self, a)
end

return parant