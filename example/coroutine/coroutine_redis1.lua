package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local coroutine = require("ccoroutine")
local redis = require("redis").init(event_loop)


redis.Connect = coroutine.coroutinize1(redis.Connect)

local count = 0
local lastShow = chuck.time.systick()

coroutine.run(function (self)
	local conn = redis.Connect("127.0.0.1",6579)
	if conn then
		local Execute = coroutine.bindcoroutinize2(conn,conn.Execute)
		for i = 1,100 do
			coroutine.run(function ()
				while true do
					local result,err = Execute("get","hw")
					if err then
						print(err)
						return
					end
					count = count + 1
				end
			end)
		end
	end
end)

timer = event_loop:AddTimer(1000,function ()
	local now = chuck.time.systick()
	local delta = now - lastShow
	lastShow = now
	print(string.format("count:%.0f/s elapse:%d",count*1000/delta,delta))
	count = 0
end)

event_loop:WatchSignal(chuck.signal.SIGINT,function()
	event_loop:Stop()
	print("stop")
end)

event_loop:Run()

