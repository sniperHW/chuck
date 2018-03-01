package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")
local queue1 = coroutine.queue()
local queue2 = coroutine.queue()

--先启动后push消息

print("start reciver first")

for i = 1,10 do
	coroutine.run(function (self,name)
		while true do
			local msg = queue1:pop()
			if nil == msg then
				break
			else
				print(name,msg)
			end
		end
		print(name,"finish")
	end,i)
end


for i = 1,10 do
	queue1:push("message:" .. i)
end
queue1:close()


print("push message first")

--先push消息后启动

for i = 1,10 do
	queue2:push("message:" .. i)
end

for i = 1,10 do
	coroutine.run(function (self,name)
		while true do
			local msg = queue2:pop()
			if nil == msg then
				break
			else
				print(name,msg)
			end
		end
		print(name,"finish")
	end,i)
end

queue2:close()
