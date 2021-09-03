local try = require "try-catch-finally"

local object

try(function()
    try(function()
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
end).catch(function (ex)
    print(ex)
end)
.finally(function ()
    if object then
        object:dispose()
    end
end)