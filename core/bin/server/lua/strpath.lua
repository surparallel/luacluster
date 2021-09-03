local strpath = {}
--获取路径  
function strpath.stripfilename(filename)  
    return string.match(filename, "(.+)/[^/]*%.%w+$") --*nix system  
    --return string.match(filename, “(.+)\\[^\\]*%.%w+$”) — windows  
end  
  
--获取文件名  
function strpath.strippath(filename)  
    return string.match(filename, ".+/([^/]*%.%w+)$") -- *nix system  
    --return string.match(filename, “.+\\([^\\]*%.%w+)$”) — *nix system  
end  
  
--去除扩展名  
function strpath.stripextension(filename)  
    local idx = filename:match(".+()%.%w+$")  
    if(idx) then  
        return filename:sub(1, idx-1)  
    else  
        return filename  
    end  
end  
  
--获取扩展名  
function strpath.getextension(filename)  
    return filename:match(".+%.(%w+)$")  
end  
--[[
local paths = "/use/local/openresty/nginx/movies/fffff.tar.gz"  
print (stripfilename(paths))  
print (strippath(paths))  
print (stripextension(paths))  
print (getextension(paths))  
]]

return strpath
