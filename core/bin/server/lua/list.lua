local List = {}

function List.new ()
    return {first = 0, last = -1}
end

function List.pushleft (list, value)
    local first = list.first - 1
    list.first = first
    list[first] = value
end

function List.pushright (list, value)
    local last = list.last + 1
    list.last = last
    list[last] = value
end

function List.popleft (list)
    local first = list.first
    if first > list.last then error("list is empty") end
    local value = list[first]
    list[first] = nil        -- to allow garbage collection
    list.first = first + 1
    return value
end

function List.popright (list)
    local last = list.last
    if list.first > last then error("list is empty") end
    local value = list[last]
    list[last] = nil         -- to allow garbage collection
    list.last = last - 1
    return value
end

function List.Insert(list, index, value)
    list.last = list.last + 1
    table.insert(list, index, value)
end

function List.GetSize(list)
    if list.first > list.last then
        return 0
    else
        return math.abs(list.last - list.first) + 1
    end
end

return List

--[[
local list = require 'list'

local mylist = list.new()

for i= 0, 10 do
    list.pushright(mylist, i)
end

list.Insert(mylist, 2, 99)
local a = 1
]]