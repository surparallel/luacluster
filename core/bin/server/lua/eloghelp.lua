local elog = require("elog")
local elogFactory = {}

function elogFactory.sys_error(...)
    elog.sys_error(string.format(...))
end

function elogFactory.sys_warn(...)
    elog.sys_warn(string.format(...))
end

function elogFactory.sys_stat(...)
    elog.sys_stat(string.format(...))
end

function elogFactory.sys_fun(...)
    elog.sys_fun(string.format(...))
end

function elogFactory.sys_details(...)
    elog.sys_details(string.format(...))
end

function elogFactory.node_error(...)
    elog.node_error(string.format(...))
end

function elogFactory.node_warn(...)
    elog.node_warn(string.format(...))
end

function elogFactory.node_stat(...)
    elog.node_stat(string.format(...))
end

function elogFactory.node_fun(...)
    elog.node_fun(string.format(...))
end

function elogFactory.node_details(...)
    elog.node_details(string.format(...))
end

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