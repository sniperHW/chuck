package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local coroutine = require("ccoroutine")
local queue1 = coroutine.queue()
local queue2 = coroutine.queue()

--先启动后push消息

print("start reciver first")

for i = 1,10 do
	coroutine.run(function ()
		local index = i
		while true do
			local err,msg = queue1:pop()
			if err == "closed" then
				break
			else
				print(index,msg)
			end
		end
		print(index,"finish")
	end)
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
	coroutine.run(function ()
		local index = i
		while true do
			local err,msg = queue2:pop()
			if err == "closed" then
				break
			else
				print(index,msg)
			end
		end
		print(index,"finish")
	end)
end

queue2:close()
