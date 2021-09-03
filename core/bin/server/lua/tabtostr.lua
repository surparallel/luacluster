
local tabtostrObj = {}
------ 把table转成string
-- sign 打印在最前面的一个标记
-- tab 待处理table
-- showAddress 是否显示table的地址
function tabtostrObj.doing(sign, tab, showAddress)

    -- 缓存table地址，防止递归死循环
    local tabs = {};
    local check = function(cur_tab, key, parentKey, level)
        local tempP = tabs[(level-1) .. parentKey]
        while tempP do
            if tempP.id == tostring(cur_tab) then
                return false;
            end
            tempP = tempP.parent;
        end

        tabs[level .. key] = {};
        tabs[level .. key].id = tostring(cur_tab);
        tabs[level .. key].parent = tabs[(level-1) .. parentKey];

        return true;
    end

    -- 处理直接传入table的情况
    if tab == nil then
        tab = sign;
        sign = "table:";
    end

    local targetType = type(tab);
    if targetType == "table" then
        local isHead = false;
        local function dump(t, tKey, space, level)
            local temp = {};
            if not isHead then
                temp = {sign or "table:"};
                isHead = true;
            end

            if tKey ~= "_fields" then
                table.insert(temp, string.format("%s{", string.rep("    ", level)));
            end
            for k, v in pairs(t) do
                local key = tostring(k);
                -- 协议返回内容
                if key == "_fields" then
                    local fields = {};
                    for fk, fv in pairs(v) do
                        fields[fk.name] = fv;
                    end
                    table.insert(temp, dump(fields, key, space, level))
                    -- 如果是table模拟的类，忽略。 以下划线开头的字段, 忽略
                elseif key == "class" or string.sub(key, 1, string.len("_")) == "_" then
                    -- 这里忽略

                elseif type(v) == "table" then
                    if check(v, key, tKey, level) then
                        if showAddress then
                            table.insert(temp, string.format("%s%s: %s\n%s", string.rep("    ", level+1), key, tostring(v), dump(v, key, space, level + 1)));
                        else
                            table.insert(temp, string.format("%s%s: \n%s", string.rep("    ", level+1), key, dump(v, key, space, level + 1)));
                        end
                    else
                        table.insert(temp, string.format("%s%s: %s (loop)", string.rep("    ", level+1), key, tostring(v)));
                    end
                else
                    table.insert(temp, string.format("%s%s: %s", string.rep("    ", level+1), key, tostring(v)));
                end
            end
            if tKey ~= "_fields" then
                table.insert(temp, string.format("%s}", string.rep("    ", level)));
            end

            return table.concat(temp, string.format("%s\n", space));
        end
        return dump(tab, "", "", 0);
    else
        return tostring(tab);
    end
end

return tabtostrObj