package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")
local queueAtoB = coroutine.queue()
local queueBtoA = coroutine.queue()


coroutine.run(function ()
	print("A start")
	for i = 1,4 do
		queueAtoB:push("A")
		print(queueBtoA:pop())
	end
	print("A finish")
end)

coroutine.run(function ()
	print("B start")
	for i = 1,4 do
		queueBtoA:push("B")
		print(queueAtoB:pop())
	end
	print("B finish")
end)