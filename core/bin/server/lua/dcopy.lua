-- Lua table deep copy
local dcopy = {}

function dcopy.clone(object)
    local lookup_table = {}
    local function _copy(object)
        if type(object) ~= "table" then
            return object
        elseif lookup_table[object] then
            return lookup_table[object]
        end
        local new_table = {}
        lookup_table[object] = new_table
        for key, value in pairs(object) do
            new_table[_copy(key)] = _copy(value)
        end
        return setmetatable(new_table, getmetatable(object))
    end
    return _copy(object)
end
 
return dcopy

--[[ 
local deepCopyTest1 = {a = 1, b = 2, c = 3}
local deepCopyTest2 = clone(deepCopyTest1)
deepCopyTest2.b = 99
print(deepCopyTest1.b)
-- 2
]]
