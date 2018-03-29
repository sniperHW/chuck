
local M = {}

local function dump_value_(v)
    if type(v) == "string" then
        v = "\"" .. v .. "\""
        return tostring(v)
    elseif type(v) == "table" or type(v) == "userdata" then
        local mt = getmetatable(v)
        if mt and mt.__name then
            return "[" .. mt.__name .."]" .. tostring(v)
        else
            return tostring(v)
        end
    else
        return tostring(v)
    end
end

--function M.dump(value, desciption, nesting)
local function _dump(value, desciption, nesting) 
    if type(nesting) ~= "number" then nesting = 5 end

    local lookupTable = {}
    local result = {}

    --local traceback = string.split(debug.traceback("", 2), "\n")
    --print("dump from: " .. string.trim(traceback[3]))


    local function dump_(value, desciption, indent, nest, keylen)
        desciption = desciption or "<var>"
        local spc = ""
        if type(keylen) == "number" then
            spc = string.rep(" ", keylen - string.len(dump_value_(desciption)))
        end
        if type(value) ~= "table" then
            result[#result +1 ] = string.format("%s%s%s = %s", indent, dump_value_(desciption), spc, dump_value_(value))
        elseif lookupTable[tostring(value)] then
            result[#result +1 ] = string.format("%s%s%s = *REF*", indent, dump_value_(desciption), spc)
        else
            lookupTable[tostring(value)] = true
            if nest > nesting then
                result[#result +1 ] = string.format("%s%s = *MAX NESTING*", indent, dump_value_(desciption))
            else

                local mt = getmetatable(value)
                if mt and mt.__name then
                    result[#result +1 ] = string.format("%s%s = [%s]{", indent, dump_value_(desciption),mt.__name)
                else
                    result[#result +1 ] = string.format("%s%s = {", indent, dump_value_(desciption))
                end
                local indent2 = indent.."    "
                local keys = {}
                local keylen = 0
                local values = {}
                for k, v in pairs(value) do
                    keys[#keys + 1] = k
                    local vk = dump_value_(k)
                    local vkl = string.len(vk)
                    if vkl > keylen then keylen = vkl end
                    values[k] = v
                end
                table.sort(keys, function(a, b)
                    if type(a) == "number" and type(b) == "number" then
                        return a < b
                    else
                        return tostring(a) < tostring(b)
                    end
                end)
                for i, k in ipairs(keys) do
                    dump_(values[k], k, indent2, nest + 1, keylen)
                end
                result[#result +1] = string.format("%s}", indent)
            end
        end
    end
    dump_(value, desciption, "- ", 1)

    local ret = ""

    for i, line in ipairs(result) do
        ret = ret .. line .. "\n"
    end

    return ret
end

function M.print(value, desciption, nesting) 
    print(_dump(value,desciption,nesting))
end

function M.str(value, desciption, nesting)
    return _dump(value,desciption,nesting)
end

return M