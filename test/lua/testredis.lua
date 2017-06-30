package.cpath = './lib/?.so;'
local chuck = require("chuck")
local redis = chuck.redis

local event_loop = chuck.event_loop.New()

local count = 0
local query_cb = nil

redis.Connect_ip4(event_loop,"127.0.0.1",6379,function (conn)
	for i = 1,1000 do

		local key = string.format("chaid:%d",i)
		--将命令插入到发送队列
		local ret = conn:Execute(query_cb,"hmset",key,"chainfo","fasdfasfasdfasdfasdfasfasdfasfasdfdsaf","skills","fasfasdfasdfasfasdfasdfasdfcvavasdfasdf")
		
		if err then
			conn:Close()
			return
		end	

	end

	query_cb = function (reply,err)
		if err then
			conn:Close()
		else
			if reply then
				count = count + 1
			else
				print("reply = nil")
			end
			local key = string.format("chaid:%d",math.random(1,1000))

			err = conn:Execute(query_cb,"hmget",key,"chainfo","skills")
			if err then
				conn:Close()
			end
		end
	end

	for i=1,1000 do
		local key = string.format("chaid:%d",math.random(1,1000))
		local err = conn:DelayExecute(query_cb,"hmget",key,"chainfo","skills")
		if err then
			conn:Close()
			return
		end
	end

end)

local timer = event_loop:AddTimer(1000,function ()
	print(count)
	count = 0
end)

event_loop:WatchSignal(chuck.signal.SIGINT,function()
	print("recv SIGINT")
	event_loop:Stop()
end)	

event_loop:Run()

