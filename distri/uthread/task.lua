local pool = require("distri.uthread.pool")
local task = {}
local currentTask

function task:new(func,...)
	local o = {}   
	o.__index = task
	setmetatable(o, o)
	o.func = func
	o.arg = table.pack(...)	  
	pool.AddTask(o)
end

return {
	New = function(func,...) task:new(func,...) end,
	Block = function(ms) pool.Block(ms) end
}