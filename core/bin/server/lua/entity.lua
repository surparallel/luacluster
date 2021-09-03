local strpath = require("strpath")
local entityFactory = {}
local bit = require("bit")

--需要回调写入事件的属性
function entityFactory.CreateSub(obj, t, name, root, rootk)
    local wrap = {}
    local rawobj = obj or {}
    wrap.__rawobj = rawobj
    wrap.__parant = t --__parant为空并且__rootk等于__name则为对象根属性
    wrap.__name = name
    wrap.__root = root
    wrap.__rootk = rootk --通过key找到对应key flag
    return setmetatable(wrap,{
        __index = function (t,k)
            return rawobj[k]
        end,

        __newindex = function (t,k,v)
            
            --事件创建和修改
            if t.__root.__KeyFlags[t.__rootk] ~= nill and t.__root.__KeyFilter ~= nil then
                for key, fun in pairs(t.__root.__KeyFilter) do
                    fun(t,k,v,rawobj[k],t.__root.__KeyFlags[t.__rootk])
                end  
            end

            if rawobj[k] == nil and type(v) == 'table' then
                if getmetatable(v) == nil then
                    rawobj[k] = entityFactory.CreateSub(v, t, k, t.__root, wrap.__rootk)
                else
                    error("An attempt was made to assign an object that cannot be serialized")
                end
            else
                rawobj[k] = v   
            end
        end
    })
end

function classname(level)
    local info = debug.getinfo(level)
    local name = strpath.strippath(info.short_src)
    return strpath.stripextension(name)
end

--New 以及其他的所有New 就是一个构造函数
function entityFactory.New(arg)

    local wrap = {}
    local rawobj = {}
    wrap.__rawobj = rawobj
    wrap.__KeyFilter = {}
    wrap.__KeyFlags = {}
    wrap.__inherit = {}

    if arg ~= nil then
        for k,v in pairs(arg) do
            rawobj[k] = v      
        end
    end

    function rawobj:testFun(arg)
        print("hello entity::"..arg)
    end

    --当注册完成时被调用返回entity id
    function rawobj:Init()
    end

    function rawobj:Destory()
    end

    function rawobj:SetKeyFlags(k, f)
        local KeyFlags = rawget(self, "__KeyFlags")
        KeyFlags[k] = f
    end

    function rawobj:AddKeyFlags(k, f)
        local KeyFlags = rawget(self, "__KeyFlags")
        if KeyFlags[k] ~= nil then
            KeyFlags[k] = bit.bor(KeyFlags[k] , f)
        else
            KeyFlags[k] = f
        end
    end

    function rawobj:ClearKeyFlags(k)
        local KeyFlags = rawget(self, "__KeyFlags")
        KeyFlags[k] = nil
    end

    function rawobj:HaveKeyFlags(k, f)
        local KeyFlags = rawget(self, "__KeyFlags")
        if KeyFlags[k] ~= nil then
            if bit.band(KeyFlags[k], f) ~= 0 then
                return true
            end
        end
        return false
    end

    --对象继承是浅copy
    function rawobj:Inherit(parant)
        local __inherit = rawget(self, "__inherit")
 
        if __inherit[parant] ~= nil then
            error("Repeated inheritance ["..parant.."]")
            return
        end

        --assert(classname(3) ~= parant, "error "..parant.." inherit self!")

        local parantFactory = require(parant)
        local parantObj = parantFactory.New()
        if parantObj == nil then
            error("Inherit error to New ["..parant.."]")
            return
        end

        if parantObj.__inherit ~= nil then
            for k, v in pairs(parantObj.__inherit) do
                if self.__inherit[k] ~= nil then
                    error("Inherit copy from ["..parant.."] __inherit have duplicate key:".. k)
                    return
                end
            end

            for k, v in pairs(parantObj.__inherit) do
                if self.__inherit[k] ~= nil then
                    self.__inherit[k] = v 
                end
            end
        end

        __inherit[parant] = parantObj

        if parantObj.__rawobj ~= nil  then
            for k, v in pairs(parantObj.__rawobj) do self.__rawobj[k] = v end
        else
            for k, v in pairs(parantObj) do self.__rawobj[k] = v end
        end

        if parantObj.__KeyFilter ~= nil then
            for k, v in pairs(parantObj.__KeyFilter) do self.__KeyFilter[k] = v end
        end

        if parantFactory.OnFreshKey ~= nil then
            local keyFilter = rawget(wrap, "__KeyFilter")
            keyFilter[parant] = parantFactory.OnFreshKey
        end
    end
    
    setmetatable(wrap,{
        __index = function (t,k)
            return rawobj[k]
        end,
        __newindex = function (t,k,v)

            local KeyFlags = rawget(wrap, "__KeyFlags")
            if KeyFlags ~= nil and KeyFlags[k] ~= nil then
                if t.__KeyFilter ~= nil then
                    for key, fun in pairs(t.__KeyFilter) do
                        fun(t,k,v,rawobj[k],KeyFlags[k])
                    end  
                end

                if rawobj[k] == nil and type(v) == 'table' then
                    if getmetatable(v) == nil then
                        rawobj[k] = entityFactory.CreateSub(v, t, k, t, k)
                    else
                        error("An attempt was made to assign an object that cannot be serialized")
                    end
                    return
                end
            end

            rawobj[k] = v
        end
    })
    return wrap
end

return entityFactory
