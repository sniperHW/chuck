local chuck   = require("chuck")
local Log     = chuck.log
local SysLog  = Log.SysLog 
local Sche    = require("distri.uthread.sche")
local LinkQue = require("distri.linkque")

local init    = 1024
local max     = 65535
local free    = LinkQue.New()
local taskque = LinkQue.New()
local total   = 0

local pool = {}

function pool.Block(ms)
	if ms > 0 and taskque:Len() > 0 then
		local node = free:Front()
		if node then
			Sche.WakeUp(node.ut)
		end
	end
	Sche.Block(ms)
end

local function GetTask()
	local task
	local ut = Sche.Running()
	local node = {ut=ut}
	free:Push(node)
	while true do
		task = taskque:Pop()
		if task then
			free:Remove(node)
			return task
		end
		Sche.Block()
	end
end

local function Do(task)
	local stack,errmsg
	if not xpcall(task.func,
		          function (err)
					errmsg = err
					stack  = debug.traceback()
		       	  end,table.unpack(task.arg))
	then
    		SysLog(Log.ERROR,string.format("error on [task:Do]:%s\n%s",errmsg,stack))
	end
end

local function ut_main()
	total = total + 1
	while true do
		Do(GetTask())
		if free:Len() >= init then
			break
		end
	end
	total = total - 1
end

function pool.AddTask(task)
	taskque:Push(task)
	local node = free:Front()
	if node then
		Sche.WakeUp(node.ut)
	elseif total < max then
		Sche.Spawn(ut_main)
	end
end

for i = 1,init do
	Sche.SpawnAndRun(ut_main)
end

return pool



