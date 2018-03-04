package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")

local co1,co2

co1 = coroutine.create(function (self)
	print(coroutine.yield())
	local i = 0
	while i < 4 do
		print(coroutine.resume_and_yield(co2,"co2"))	
		i = i + 1
	end
	print("co1 finish")
end)



co2 = coroutine.create(function (self)
	local i = 0
	coroutine.resume(co1,"co1")
	while i < 4 do
		print(coroutine.resume_and_yield(co1,"co1"))
		i = i + 1
	end
	coroutine.resume(co1,"co1")
end)



print("start loop")
coroutine.sche()
print("end")