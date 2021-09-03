local list = require 'list'

local mylist = list.new()

for i= 0, 10 do
    list.pushright(mylist, i)
end

list.Insert(mylist, 2, 99)
local a = 1