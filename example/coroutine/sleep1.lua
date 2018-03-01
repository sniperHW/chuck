package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")

--没有设置eventLoop,所以sleep只是执行yiled_a_while

coroutine.create(function ()
	for i = 1,10 do
		coroutine.sleep(1000)
		print("co1")
	end
	print("co1 finish")
end)

coroutine.create(function ()
	for i = 1,10 do
		coroutine.sleep(1000)
		print("co2")
	end
	print("co2 finish")
end)

coroutine.sche()

--跟前面的输出做比较

coroutine.run(function ()
	for i = 1,10 do
		coroutine.sleep(1000)
		print("co3")
	end
	print("co3 finish")
end)

coroutine.run(function ()
	for i = 1,10 do
		coroutine.sleep(1000)
		print("co4")
	end
	print("co4 finish")
end)