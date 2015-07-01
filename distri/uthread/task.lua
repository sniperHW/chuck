local pool = require("distri.uthread.sche").Pool
local task = {}
local currentTask

function task:new(func,...)
	local o = {}   
	o.__index = task
	setmetatable(o, o)
	o.func = func
	o.arg = {...}	  
	pool.AddTask(o)
end

local function Wrap(func)
	return function (...)
		task:new(func,...)
	end
end

return {
	New  = function(func,...) task:new(func,...) end,
	Wrap = Wrap,
}