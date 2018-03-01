package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")

local co1,co2

co1 = coroutine.create(function (self)
	print(coroutine.yield())
	print(co1.a)
	co1.a = "b"
	print(co1.a)
	local i = 0
	while i < 4 do
		print(coroutine.resume_and_yield(co2,"co2"))	
		i = i + 1
	end
	print("co1 finish",co1.a)
end)

co1.a = "a"

co2 = coroutine.create(function (self)
	local i = 0
	print(co1.a)
	coroutine.resume(co1,"co1")
	co1.a = "c"
	while i < 4 do
		print(coroutine.resume_and_yield(co1,"co1"))
		i = i + 1
	end
	coroutine.resume(co1,"co1")
	print("co2 finish",co1.a)
end)



print("start loop")
coroutine.sche()
print("end")