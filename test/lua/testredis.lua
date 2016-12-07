package.cpath = './lib/?.so;'
local chuck = require("chuck")
local redis = chuck.redis

local event_loop = chuck.event_loop.New()

local count = 0
local query_cb = nil

redis.Connect_ip4(event_loop,"127.0.0.1",6379,function (conn)
	for i = 1,1000 do
		local key = string.format("chaid:%d",i)
		conn:Execute(query_cb,"hmset",key,"chainfo","fasdfasfasdfasdfasdfasfasdfasfasdfdsaf","skills","fasfasdfasdfasfasdfasdfasdfcvavasdfasdf")

	end

	query_cb = function (reply)
		if reply then
			count = count + 1
		else
			print("reply = nil")
		end
		local key = string.format("chaid:%d",math.random(1,1000))
		conn:Execute(query_cb,"hmget",key,"chainfo","skills")
	end

	for i=1,1000 do
		local key = string.format("chaid:%d",math.random(1,1000))
		conn:Execute(query_cb,"hmget",key,"chainfo","skills")
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

