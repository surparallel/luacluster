local bit32 = bit32
if bit32 == nil then
    bit32 = bit
end

---@class Entity
local entity = {}

--需要回调写入事件的属性
function entity.CreateSub(obj, t, name, root, rootk)
    local wrap = {}
    local rawobj = obj or {}

    wrap.__rawobj = rawobj
    wrap.__parant = t --__parant为空并且__rootk等于__name则为对象根属性
    wrap.__name = name
    wrap.__root = root
    wrap.__rootk = rootk --通过key找到对应key flag

    for k, v in pairs(obj) do
        if type(v) == 'table' then
            obj[k] = entity.CreateSub(v, wrap, k, root, rootk)
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
                    rawobj[k] = entity.CreateSub(v, t, k, t.__root, wrap.__rootk)
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
function entity.NewClass()

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
    wrap.__class = "Entity"

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
                entity.Recombination(v, out[__name])
            end
        end
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

    function rawobj:initialize(...)
    end

    --注意类继承和生成对象都是浅copy，可能导致修改指针的错误
    function rawobj:Inherit(parant)
        if self.__inherit[parant] ~= nil then
            error("Repeated inheritance ["..parant.."]")
            return
        end

        local parantClass = require(parant)

        if parantClass == nil then
            error("Inherit error to New ["..parant.."]")
            return
        end

        --这会导致只有继承或者创建对象的时候才知道名字，但多加个参数就显得很不优雅
        parantClass:EntityClass(parant)

        for k, v in pairs(parantClass.__allparant) do
            if self.__allparant[k] ~= nil then
                error("Repeated inheritance ["..k.."]")
                return
            end
            self.__allparant[k] = v
        end 

        self.__inherit[parant] = parantClass

        if self.__allparant[parant] ~= nil then
            error("Repeated inheritance ["..parant.."]")
            return    
        end
        self.__allparant[parant] = parantClass

        if parantClass.__rawobj ~= nil  then
            for k, v in pairs(parantClass.__rawobj) do
                if self.__rawobj[k] ~= nil then
                    self.__rawobj[k] = v
                end
            end
        else
            for k, v in pairs(parantClass) do
                if self.__rawobj[k] ~= nil then
                    self.__rawobj[k] = v
                end
            end
        end

        if parantClass.__FlagFilter ~= nil then
            for k, v in pairs(parantClass.__FlagFilter) do
                for key, fun in pairs(v) do
                    self:AddOneFlagFilter(k, fun)
                end
            end
        end

        if parantClass.__KeyFlags ~= nil then
            for k, v in pairs(parantClass.__KeyFlags) do self.__KeyFlags[k] = v end
        end

        if parantClass.__FlagFilterFun ~= nil then
            for k, v in pairs(parantClass.__FlagFilterFun) do self.__FlagFilterFun[k] = v end
        end
    end

    --复制类对象
    function rawobj:CopyObject(name)       

        local parantClass = require(name)
        if parantClass == nil then
            error("New ["..name.."]")
            return
        end

        self.Inherit = nil
        rawset(self, "__obj", name)
        rawset(self, "__class", nil)

        for k, v in pairs(parantClass.__allparant) do
            if self.__allparant[k] ~= nil then
                error("Repeated inheritance ["..k.."]")
                return
            end
            self.__allparant[k] = v
        end

        for k, v in pairs(parantClass.__inherit) do
            self.__inherit[k] = v
        end

        if self.__allparant[parant] ~= nil then
            error("Repeated inheritance ["..parant.."]")
            return    
        end

        rawset(self, "__parantClass", parantClass)

        if parantClass.__rawobj ~= nil  then
            for k, v in pairs(parantClass.__rawobj) do
                self.__rawobj[k] = v
            end
        else
            for k, v in pairs(parantClass) do
                self.__rawobj[k] = v
            end
        end

        if parantClass.__FlagFilter ~= nil then
            for k, v in pairs(parantClass.__FlagFilter) do
                for key, fun in pairs(v) do
                    self:AddOneFlagFilter(k, fun)
                end
            end
        end

        if parantClass.__KeyFlags ~= nil then
            for k, v in pairs(parantClass.__KeyFlags) do self.__KeyFlags[k] = v end
        end

        if parantClass.__FlagFilterFun ~= nil then
            for k, v in pairs(parantClass.__FlagFilterFun) do self.__FlagFilterFun[k] = v end
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
                        t.__rawobj[k] = entity.CreateSub(v, t, k, t, k)
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
--[[
        __ipairs = function(t)
            return ipairs(t.__rawobj)
          end,

        --__pairs会导致调试器的循环失效，显示错误的数据
        __pairs = function(t)
            return pairs(t.__rawobj)
          end,]]
    })
    return wrap
end

---全局的关键字
---@param name string @要继承的类名字
function class(...)
    local args = {...}

    --创建当前的类
    local myEntity = entity.NewClass()
    
    --按顺序依次继承
    for i, v in ipairs(args) do
        if v ~= "" then
            myEntity:Inherit(v)
        else
            error("error entity.NewAndInherit arg "..i.." is nil")
        end
    end
    
    return myEntity
end

---全局的关键字
---@param name String @类的名字
---@param ... any @初始化函数的参数
function new(name, ...)

    if name == nil then
        error("new name is nil")
    end

    local args = {...}
    --创建当前的类
    local myEntity = entity.NewClass()
    myEntity:CopyObject(name)
    myEntity:initialize(args)
    return myEntity
end

return entity
