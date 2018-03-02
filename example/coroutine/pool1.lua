package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

--local event_loop = require("chuck").event_loop.New()
local coroutine = require("ccoroutine").setEventLoop(event_loop)

local pool = coroutine.pool(1,10,function (msg)
	print(coroutine.running(),msg)
end)

for i = 1,10 do
	pool:push(i)
end

pool:close()

