package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local event_loop = require("chuck").event_loop.New()
local coroutine = require("ccoroutine").setEventLoop(event_loop)

coroutine.run(function ()
	for i = 1,10 do
		coroutine.sleep(1000)
		print("co3")
	end
	print("co3 finish")
end)

coroutine.run(function ()
	for i = 1,10 do
		coroutine.sleep(2000)
		print("co4")
	end
	print("co4 finish")
end)

event_loop:Run()