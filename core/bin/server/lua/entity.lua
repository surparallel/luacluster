
local bit32 = bit32
if bit32 == nil then
    bit32 = bit
end
local entityFactory = {}

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
            if t.__root.__KeyFlags[t.__rootk] ~= nil and t.__root.__FlagFilter[t.__root.__KeyFlags[t.__rootk]] ~= nil then
                for key, fun in pairs(t.__root.__FlagFilter[t.__root.__KeyFlags[t.__rootk]]) do
                    local myRoot = t
                    if t.__root ~= nil then
                        myRoot = t.__root
                    end
                    local rootKey = k
                    if myRoot.__rootk ~= nil then
                        rootKey = myRoot.__rootk
                    end
                    fun(myRoot,rootKey)
                end
            end

            if type(v) == 'table' then
                if getmetatable(v) == nil then
                    rawobj[k] = entityFactory.CreateSub(v, t, k, t.__root, wrap.__rootk)
                else
                    if v.__rawobj == nil or v.__entity == nil then
                        error("CreateSub An attempt was made to assign an object that cannot be serialized "..k)
                    else
                        rawobj[k] = v
                        return
                    end
                end
            else
                rawobj[k] = v
                return
            end
        end,

        __ipairs = function(t)
            return ipairs(t.__rawobj)
          end,
        __pairs = function(t)
            return pairs(t.__rawobj)
          end,
    })
end

--CreateObj 以及其他的所有CreateObj 就是一个构造函数
function entityFactory.New()

    local wrap = {}
    local rawobj = {}
    wrap.__rawobj = rawobj
    wrap.__FlagFilter = {} --key对应的KeyFilterFlag下的所有fun
    wrap.__FlagFilterFun = {} --单一flag对应的函数防止误释放
    wrap.__KeyFlags = {}
    wrap.__inherit = {}
    wrap.__allparant = {}
    wrap.__entity = 1
    wrap.__FreshKey = {}

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

    function rawobj:AddOneFlagFilter(flag, fun)
        if self.__FlagFilter[flag] == nil then
            self.__FlagFilter[flag] = {}       
        end
        self.__FlagFilter[flag][tostring(fun)] = fun
        
    end

    function rawobj:AddFlagFilter(flag, fun)
        for iflag, ifun in pairs(self.__FlagFilter) do
            if bit32.band(iflag, flag) ~= 0 then
                ifun[tostring(fun)] = fun
            end
        end
    end

    function rawobj:AddFlagFun(flag, fun) --publice

        local isempty = 0
        if self.__FlagFilter[flag] == nil then
            isempty = 1
        end

        --将flag中每个标记单独取出来，放入__FlagFilterFun列表中
        local cflag = 1
        local tflag = flag
        for i = 1, 32, 1 do
            if bit32.band(tflag , 1) == 1 then

                if self.__FlagFilterFun[cflag] == nil then
                    self.__FlagFilterFun[cflag] = {}
                    self.__FlagFilterFun[cflag][tostring(fun)] = fun
                else
                    self.__FlagFilterFun[cflag][tostring(fun)] = fun
                    self:AddFlagFilter(cflag, fun)

                    if isempty == 1 then
                        for k, v in pairs(self.__FlagFilterFun[cflag][tostring(fun)]) do
                            self:AddOneFlagFilter(flag, v)
                        end
                    end

                end   
            end

            tflag = bit32.rshift(tflag, 1)
            cflag = bit32.lshift(cflag, 1)

            if tflag == 0 then
                break
            end
        end

        self:AddOneFlagFilter(flag, fun)
    end

    function rawobj:SetKeyFlags(k, f)
        local KeyFlags = rawget(self, "__KeyFlags")
        KeyFlags[k] = f
    end

    function rawobj:AddKeyFlags(k, f)
        local KeyFlags = rawget(self, "__KeyFlags")
        if KeyFlags[k] ~= nil then
            KeyFlags[k] = bit32.bor(KeyFlags[k] , f)
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
            if bit32.band(KeyFlags[k], f) ~= 0 then
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

        if parantObj.__FlagFilter ~= nil then
            for k, v in pairs(parantObj.__FlagFilter) do
                for key, fun in pairs(v) do
                    self:AddOneFlagFilter(k, fun)
                end
            end
        end

        if parantObj.__KeyFlags ~= nil then
            for k, v in pairs(parantObj.__KeyFlags) do self.__KeyFlags[k] = v end
        end

        if parantObj.__FlagFilterFun ~= nil then
            for k, v in pairs(parantObj.__FlagFilterFun) do self.__FlagFilterFun[k] = v end
        end
    end

    setmetatable(wrap,{
        __index = function (t,k)
            return t.__rawobj[k]
        end,
        __newindex = function (t,k,v)

            if t.__KeyFlags[k] ~= nil and t.__FlagFilter[t.__KeyFlags[k]] ~= nil then
                for key, fun in pairs(t.__FlagFilter[t.__KeyFlags[k]]) do
                    fun(t,k)
                end

                if type(v) == 'table' then
                    if getmetatable(v) == nil then
                        t.__rawobj[k] = entityFactory.CreateSub(v, t, k, t, k)
                        return
                    else
                        if v.__rawobj == nil or v.__entity == nil then
                            error("An attempt was made to assign an object that cannot be serialized "..k)
                        else
                            t.__rawobj[k] = v
                            return
                        end
                    end
                else
                    t.__rawobj[k] = v
                    return
                end
            end
            t.__rawobj[k] = v
        end,

        __ipairs = function(t)
            return ipairs(t.__rawobj)
          end,

        --__pairs会导致调试器的循环失效，显示错误的数据
        __pairs = function(t)
            return pairs(t.__rawobj)
          end,
    })
    return wrap
end

return entityFactory
