
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

    for k, v in pairs(obj) do
        if type(v) == 'table' then
            obj[k] = entityFactory.CreateSub(v, wrap, k, root, rootk)
        end
    end

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

--CreateObj 以及其他的所有CreateObj 就是一个构造函数
function entityFactory.New()

    local wrap = {}
    local rawobj = {}
    wrap.__rawobj = rawobj
    wrap.__KeyFilter = {}
    wrap.__KeyFlags = {}
    wrap.__inherit = {}
    wrap.__allparant = {}

    function rawobj:CopyArg(arg)
        if arg ~= nil then
            for k,v in pairs(arg) do
                self.__rawobj[k] = v
            end
        end
    end

    function rawobj:EntityClass(class)
        rawset(self, "__class", class)
    end

    function rawobj:DoInheritFun(sub, fun, ...)    
        if(sub.__inherit ~= nil) then
            for k, v in pairs(sub.__inherit) do
                self:DoInheritFun(v, fun)
            end
        end
        
        if sub[fun] ~= nil then
            sub[fun](self, ...)
        end
    end
    
    function rawobj:Recombination(obj, out)
        local __rawobj = rawget(obj, "__rawobj")
        local __name = rawget(obj, "__name")
        out[__name] = __rawobj
        for k, v in pairs(out[__name]) do
            if type(v) == "table" then
                entityFactory.Recombination(v, out[__name])
            end
        end
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
        if self.__inherit[parant] ~= nil then
            error("Repeated inheritance ["..parant.."]")
            return
        end

        local parantFactory = require(parant)
        local parantObj = parantFactory.New()
        if parantObj == nil then
            error("Inherit error to New ["..parant.."]")
            return
        end

        for k, v in pairs(parantObj.__allparant) do
            if self.__allparant[k] ~= nil then
                error("Repeated inheritance ["..k.."]")
                return
            end
            self.__allparant[k] = v
        end 

        self.__inherit[parant] = parantObj

        if self.__allparant[parant] ~= nil then
            error("Repeated inheritance ["..parant.."]")
            return    
        end
        self.__allparant[parant] = parantObj

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
            return t.__rawobj[k]
        end,
        __newindex = function (t,k,v)
            if t.__KeyFlags ~= nil and t.__KeyFlags[k] ~= nil then
                if t.__KeyFilter ~= nil then
                    for key, fun in pairs(t.__KeyFilter) do
                        fun(t,k,v,t.__rawobj[k],t.__KeyFlags[k])
                    end  
                end

                if t.__rawobj[k] == nil and type(v) == 'table' then
                    if getmetatable(v) == nil then
                        t.__rawobj[k] = entityFactory.CreateSub(v, t, k, t, k)
                    else
                        error("An attempt was made to assign an object that cannot be serialized")
                    end
                    return
                end
            end

            t.__rawobj[k] = v
        end
    })
    return wrap
end

return entityFactory
