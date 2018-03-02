package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local event_loop = require("chuck").event_loop.New()
local coroutine = require("ccoroutine").setEventLoop(event_loop)

local pool = coroutine.pool(1,10,function (msg)
	print(coroutine.running(),msg)
	--模拟异步任务
	coroutine.sleep(1000)
end)

for i = 1,20 do
	pool:push(i)
end

--当pool中所有coroutine结束后调用回调函数终止event_loop
pool:close(function ()
	event_loop:Stop()
end)

event_loop:Run()