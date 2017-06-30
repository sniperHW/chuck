local M = {}

--建立对obj的弱引用
M.Ref = function (obj)
	if not obj then
		return nil
	end
	local o = {__mode="v"}

	o.__pairs = function()
		local iter = function (_,idx)
			return next(o.ref,idx)
		end
		return iter,o.ref,nil
	end

	o.__index = obj
	o.__newindex = obj
	o.ref = obj
	o = setmetatable(o,o)
	return o
end

--[[
local function tableIter(t,idx)
	local _idx = idx
	while true do
		local k,v = next(t,_idx)
		if not k then
			return nil,nil
		end
		if k ~= "__mode" and k ~= "__pairs" then
			return k,v
		else
			_idx = k
		end
	end
end
]]--

--返回一个弱key表
M.KeyTable = function ()
	local mt = {__mode="k"}
	local o = {}
	o = setmetatable(o,mt)	
	return o
end

--返回一个弱value表
M.ValueTable = function ()
	local mt = {__mode="v"}
	local o = {}
	o = setmetatable(o,mt)	
	return o
end

--返回一个弱key,value表
M.KeyValueTable = function ()
	local mt = {__mode="kv"}
	local o = {}
	o = setmetatable(o,mt)	
	return o
end


return M