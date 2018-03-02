package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")

local pool = coroutine.pool(1,10)

for i = 1,10 do
	pool:addTask(function ()
		print(coroutine.running(),i)
	end)
end

pool:close()

