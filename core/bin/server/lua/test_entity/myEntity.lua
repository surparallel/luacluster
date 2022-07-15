---@class MyEntity
local myEntity = class("parant", "parant2")

myEntity:SetKeyFlags("a", 1)
myEntity.a = {"f", "h",{"l","m"}}
myEntity.cc.a = 1

---@param a Table
function myEntity:Init(a)
    print("myEntity init", self, a)
end

function myEntity:fun1() 
    print("myEntity.fun1",self)
end

return myEntity
