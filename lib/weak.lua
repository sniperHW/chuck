local M = {}

--建立对obj的弱引用

--[[

注意:

如果被引用对象obj
1:存在__call元方法
2:存在一个函数成员func

当执行obj()或obj:func()时

传入的self对象是weak包装对象而不是原来的obj,

如果需要对self做特殊操作(例如判断self对象是否跟某对象相等),则应该先判断self是否一个弱引用，如果是
则应该通过refObj来获得被指向对象,例如下面这样

self = self.isWeakRef and self.refObj or self


]]--


M.Ref = function (obj)
	if not obj then
		error("obj == nil")
	end

	local obj_type = type(obj) 
	if obj_type ~= "table" then
		error("obj must be a table")
	end

	local o = {__mode="v"}

	o.__pairs = function()
		local iter = function (_,idx)
			return next(obj,idx)
		end
		return iter,obj,nil
	end

	o.__index = obj
	o.__newindex = obj
	local mt = getmetatable(obj)
	if mt then
		o.__call = mt.__call
	end
	o.__len = function ()
		return #obj
	end
	o.isWeakRef = true
	o.refObj = obj
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