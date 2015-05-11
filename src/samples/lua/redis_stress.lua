local chuck = require("chuck")
local engine = chuck.engine()
local err,client = chuck.redis.Connect(engine,"127.0.0.1",6379)

local function sigint_handler()
	print("recv sigint")
	engine:Stop()
end

local signaler = signal.signaler(signal.SIGINT)

if client then
	local count = 0

	local function gen_callback(cb,ud)
		return function (conn,reply)
			cb(conn,reply,ud)
		end
	end

	local function query_cb(conn,reply,id)
		count = count + 1
		local cmd = string.format("hmget chaid:%d chainfo skill",id)
		client:Execute(cmd,gen_callback(query_cb,id))		
	end


	for i = 1,1000 do
		local cmd = string.format("hmset chaid:%d chainfo %s skills %s",i,
								  "fasdfasfasdfasdfasdfasfasdfasfasdfdsaf",
								  "fasfasdfasdfasfasdfasdfasdfcvavasdfasdf")
		client:Execute(cmd)
	end

	for i = 1,1000 do
		local cmd = string.format("hmget chaid:%d chainfo skills",i)
		client:Execute(cmd,gen_callback(query_cb,i))
	end
	local last = chuck.systick()
	chuck.RegTimer(engine,1000,
				   function() 
				   		collectgarbage("collect") 
				   		local now = chuck.systick()
				   		print("hmget:" .. count*1000/(now-last) .. "/s")
				   		last = now
				   		count = 0 
				   end)
	signaler:Register(engine,sigint_handler)
	engine:Run()

else
	print(err)
end




