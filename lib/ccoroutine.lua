local chuck = require("chuck")
local fifo  = require("fifo")

local coroutine = chuck.coroutine

local event_loop

local M = {}

local waitGroup = {}
waitGroup.__index = waitGroup

function M.waitGroup()
	local o = {}
	o = setmetatable(o,waitGroup)
	o.counter = 0
	o.waitCount = 0
	o.waits = {}
	return o
end

function waitGroup:wait()
	local current = coroutine.running()
	if current then
		if self.counter < self.waitCount then
			table.insert(self.waits,current)
			coroutine.yield(current)
		end
	else
		error("wait must call under coroutine context")
	end
end

function waitGroup:count()
	return self.counter
end

function waitGroup:done()
	self.counter = self.counter + 1
	if self.counter >= self.waitCount then
		for k,v in pairs(self.waits) do
			coroutine.resume(v)
		end
		self.waits = {}
	end	
end

function waitGroup:add()
	self.waitCount = self.waitCount + 1
end

local queue = {}
queue.__index = queue

function M.queue()
	local o = {}
	o = setmetatable(o,queue)
	o.message_queue = fifo.new()
	o.waits = fifo.new()
	return o
end

function queue:push(msg)
	if nil == msg then
		return error("queue push nil")
	end

	if self.closed then
		return "closed"
	else
		if self.message_queue:empty() and (not self.waits:empty()) then
			local head = self.waits:pop()
			coroutine.resume(head,msg)
		else
			self.message_queue:push(msg)
		end
	end
end

function queue:pop()
	local current = coroutine.running()
	if current == nil then
		return error("pop must call in coroutine context")
	end
	local msg = self.message_queue:pop()
	if msg then
		return msg
	else
		if self.closed then
			return nil
		else
			self.waits:push(current)
			return coroutine.yield()
		end		
	end
end

function queue:close()
	if not self.closed then
		self.closed = true
		if self.message_queue:empty() then
			while true do 
				local co = self.waits:pop()
				if nil == co then
					break
				else
					coroutine.resume(co)
				end
			end
		end
	end
end

--丢弃队列中所有内容，强制唤醒等待中的coroutine
function queue:forceClose()
	if not self.closed then
		self.closed = true
		self.message_queue:clear()
		while true do 
			local co = self.waits:pop()
			if nil == co then
				break
			else
				coroutine.resume(co)
			end
		end		
	end	
end


function queue:isClosed()
	return self.closed
end


local pool = {}
pool.__index = pool


local function pool_new_coroutine(self,count)
	self.startCount = self.startCount + count
	for i = 1,count do
		self.waitGroup:add()
		M.run(function (co)
			self.count = self.count + 1
			self.startCount = self.startCount - 1
			while true do
				local task = self.taskQueue:pop()
				if task then
					util.pcall(task)
				else
					break
				end
			end
			self.count = self.count - 1
			self.waitGroup:done()
		end)
	end
end

function M.pool(initCount,maxCount)
	assert(initCount,"initCount == nil")
	assert(maxCount,"maxCount == nil")
	assert(initCount >= 0,"initCount should >= 0")
	assert(maxCount > 0 and maxCount >= initCount,"maxCount should >= maxCount")
	local o = {}
	o = setmetatable(o,pool)
	o.maxCount = maxCount
	o.count = 0   		--已经进入主函数的coroutine数量
	o.startCount = 0    --已经创建，但尚未进入主函数的coroutine数量
	o.taskQueue = M.queue()
	o.waitGroup = M.waitGroup()
	pool_new_coroutine(o,initCount)
	return o
end

function pool:addTask(task)
	assert(type(task) == "function","task should be a function")	
	if not self.closed then
		local taskQueue = self.taskQueue
		if not taskQueue:isClosed() then
			if taskQueue.waits:empty() and self.startCount + self.count < self.maxCount then
				pool_new_coroutine(self,1)
			end
			taskQueue:push(task)
		end
	end
end

function pool:close(onClose)
	if not self.closed then
		self.taskQueue:close()
		self.closed = true
		if onClose then
			M.run(function ()
				self.waitGroup:wait()
				onClose()
			end)
		end
	end
end

--丢弃队列中所有内容，强制终止pool中所有coroutine
function pool:forceClose(onClose)
	if not self.closed then
		self.taskQueue:forceClose()
		self.closed = true
		if onClose then
			M.run(function ()
				self.waitGroup:wait()
				onClose()
			end)
		end
	end
end

function M.setEventLoop(eventLoop)
	event_loop = eventLoop
	return M
end

function M.sleep (timeout)
	local fff = function ()
		local timer
		local current = coroutine.running()
		if current == nil then
			return
		end
		if event_loop == nil then
			timeout = 0
		else
			if type(timeout) == "number" then
				if timeout <= 0 then
					timeout = 0
				end
			else
				timeout = 0
			end
		end

		if timeout == 0 then
			coroutine.yield_a_while()
		else
			timer = event_loop:AddTimer(timeout,function ()
	            timer = nil  --timer可以被gc了              
	            coroutine.resume(current)
	            return -1    --非重复定时器，触发之后要立即清除				
			end)
			return coroutine.yield()
		end
	end
	return fff()
end

--创建coroutine插入到readyList中
function M.create(...)
	return coroutine.new(...)
end

--创建coroutine插入到readyList中，如果当前在主线程中立刻调度执行
function M.run(...)
	coroutine.resume(coroutine.new(...))
end

function M.sche(...)
	return coroutine.sche(...)
end

function M.running(...)
	return coroutine.running(...)
end

function M.yield(...)
	assert(coroutine.running(),"yield must call under coroutine context")
	return coroutine.yield(...)
end

function M.resume(...)
	return coroutine.resume(...)
end

function M.resume_and_yield(...)
	return coroutine.resume_and_yield(...)
end

--将function(...,callback)形式的函数coroutine化
function M.coroutinize1(f)
	local ff = function (...)
		local fff = function (current,...)	
			local param = table.pack(...)
			table.insert(param,function (...)                  
	            coroutine.resume(current, ...) 			
			end)
			f(table.unpack(param))
			return coroutine.yield()
		end
		return fff(coroutine.running(),...)
	end
	return ff
end

--将function(callback,...)形式的函数coroutine化
function M.coroutinize2(f)
	local ff = function (...)
		local fff = function (current,...)	
			f(function (...)            
	            coroutine.resume(current, ...) 			
			end,...)
			return coroutine.yield()
		end
		return fff(coroutine.running(),...)
	end
	return ff
end


--将obj.func(...,callback)形式的函数coroutine化
function M.bindcoroutinize1(obj,f)
	local o = obj
	local ff = function (...)
		local fff = function (current,...)	
			local param = table.pack(...)
			table.insert(param,function (...)              
	            coroutine.resume(current, ...) 			
			end)
			f(o,table.unpack(param))
			return coroutine.yield()
		end
		return fff(coroutine.running(),...)
	end
	return ff
end

local count = 0

--将obj.func(callback,...)形式的函数coroutine化
function M.bindcoroutinize2(obj,f)
	local o = obj
	local ff = function (...)
		local fff = function (current,...)	
			f(o,function (...)                
	            coroutine.resume(current, ...) 			
			end,...)
			return coroutine.yield()
		end
		return fff(coroutine.running(),...)
	end
	return ff
end


return M