local functionType = "function"
local istry = 1 --关闭或开启try
--- 
-- @param tryBlock The block of code to execute as the try block.
--
-- @return A table that can be used to chain try/catch/finally blocks. (Call .catch or .finally of the return value)
--
local function try (tryBlock)
	local status, err = true, nil

	if type(tryBlock) == functionType then
		if istry == 1 then
			status, err = xpcall(tryBlock, debug.traceback)
		else 
			tryBlock()
		end
	end

	local finally = function (finallyBlock, catchBlockDeclared)
		if type(finallyBlock) == functionType then
			finallyBlock()
		end
		
		if not catchBlockDeclared and not status then
			error(err)
		end
	end

	local catch = function (catchBlock)
		local catchBlockDeclared = type(catchBlock) == functionType;

		if not status and catchBlockDeclared then
			local ex = err or "unknown error occurred"
			catchBlock(ex)
		end

		return {
			finally = function(finallyBlock)
				finally(finallyBlock, catchBlockDeclared)
			end
		}
	end

	return
	{
		catch = catch,
		finally = function(finallyBlock)
			finally(finallyBlock, false)
		end
	}
end

return try

--[[
local try = require "try-catch-finally"

local object

try(function ()
    try(function ()
        object = Object()
        object:doRiskyStuff()     
    end)
    .catch(function (ex)
        print(ex)
        error(ex)
    end)
    .finally(function ()
        if object then
            object:dispose()
        end
    end)

end)
.catch(function (ex)
    print(ex)
end)
.finally(function ()
    if object then
        object:dispose()
    end
end)
]]