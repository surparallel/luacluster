local elog = require("elog")
local elogFactory = {}

function elogFactory.error(...)
    elog.error(string.format(...))
end

function elogFactory.warn(...)
    elog.warn(string.format(...))
end

function elogFactory.stat(...)
    elog.stat(string.format(...))
end

function elogFactory.fun(...)
    elog.fun(string.format(...))
end

function elogFactory.details(...)
    elog.details(string.format(...))
end

return elogFactory