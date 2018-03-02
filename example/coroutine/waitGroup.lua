package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")

local waitGroup = coroutine.waitGroup(10)

for i = 1,10 do
	coroutine.run(function (self,name)
		waitGroup:add()
		print(name,"finish")
	end,i)
end


print("count",waitGroup:count())
coroutine.run(function ()
	waitGroup:wait()
	print("final finish")
end)


waitGroup = coroutine.waitGroup(10)

for i = 1,10 do
	coroutine.create(function (self,name)
		waitGroup:add()
		print(name,"finish")
	end,i)
end

print("count",waitGroup:count())
coroutine.create(function ()
	waitGroup:wait()
	print("final finish")
end)

print("sche")
coroutine.sche()