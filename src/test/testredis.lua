local chuck = require("chuck")
local redis = chuck.redis

local event_loop = chuck.event_loop.New()

local count = 0
local query_cb = nil

redis.Connect_ip4(event_loop,"127.0.0.1",6379,function (conn)
	for i = 1,1000 do
		local cmd = string.format("hmset chaid:%d chainfo %s skills %s",i,
								  "fasdfasfasdfasdfasdfasfasdfasfasdfdsaf",
								  "fasfasdfasdfasfasdfasdfasdfcvavasdfasdf")
		conn:Execute(cmd)
	end
	query_cb = function (reply)
		if reply then
			count = count + 1
		else
			print("reply = nil")
		end
		local cmd = string.format("hmget chaid:%d chainfo skills",math.random(1,1000))
		conn:Execute(cmd,query_cb)
	end

	for i=1,1000 do
		local cmd = string.format("hmget chaid:%d chainfo skills",math.random(1,1000))
		conn:Execute(cmd,query_cb)
	end

end)

local timer = event_loop:AddTimer(1000,function ()
	print(count)
	count = 0
end)

event_loop:Run()

