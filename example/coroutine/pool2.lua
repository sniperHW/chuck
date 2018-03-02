package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local event_loop = require("chuck").event_loop.New()
local coroutine = require("ccoroutine").setEventLoop(event_loop)

local pool = coroutine.pool(1,10)

for i = 1,20 do
	pool:addTask(function ()
		print(coroutine.running(),i)
		--模拟异步任务
		coroutine.sleep(1000)
	end)
end

--当pool中所有coroutine结束后调用回调函数终止event_loop
pool:close(function ()
	event_loop:Stop()
end)

event_loop:Run()