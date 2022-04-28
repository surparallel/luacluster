-- Lua table deep copy
local dcopy = {}
function dcopy.clone(tDest, tSrc)
    if next(tSrc) ~= nil  then
        for key,value in tSrc do
            if type(value)=='table' and next(value) ~= nil then
                tDest[key] = {}
                dcopy.clone(tDest[key],value)
            else
                tDest[key]=value
            end
        end
    end
end
return dcopy

--[[ 
local deepCopyTest1 = {a = 1, b = 2, c = 3}
local deepCopyTest2 = clone(deepCopyTest1)
deepCopyTest2.b = 99
print(deepCopyTest1.b)
-- 2
]]
