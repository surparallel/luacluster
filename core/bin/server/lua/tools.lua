
local tools = {}

--获取准确小数
-- num 源数字
--n 位数
function tools.GetPreciseDecimal(num, n)
    if	type(num) ~= "number" then
        return num
    end
    n = n or 0
    n = math.floor(n)
    if	n < 0 then n = 0 end
    local decimal = 10 ^ n
    local temp = math.floor(num * decimal)
    return temp / decimal
end

return tools
