local chuck = require("chuck")

local coroutine = chuck.coroutine

local event_loop

local M = {}

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
	local ff = function (...)
		local fff = function (current,...)	
			local param = table.pack(...)
			table.insert(param,function (...)              
	            coroutine.resume(current, ...) 			
			end)
			f(obj,table.unpack(param))
			return coroutine.yield()
		end
		return fff(coroutine.running(),...)
	end
	return ff
end

local count = 0

--将obj.func(callback,...)形式的函数coroutine化
function M.bindcoroutinize2(obj,f)
	local ff = function (...)
		local fff = function (current,...)	
			f(obj,function (...)                
	            coroutine.resume(current, ...) 			
			end,...)
			return coroutine.yield()
		end
		return fff(coroutine.running(),...)
	end
	return ff
end


return M