package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")

local waitGroup = coroutine.waitGroup()

for i = 1,10 do
	waitGroup:add()
	coroutine.run(function (self,name)
		print(name,"finish")
		waitGroup:done()
	end,i)
end


print("count",waitGroup:count())
coroutine.run(function ()
	waitGroup:wait()
	print("final finish")
end)


waitGroup = coroutine.waitGroup()

for i = 1,10 do
	waitGroup:add()
	coroutine.create(function (self,name)
		print(name,"finish")
		waitGroup:done()
	end,i)
end

print("count",waitGroup:count())
coroutine.create(function ()
	waitGroup:wait()
	print("final finish")
end)

print("sche")
coroutine.sche()