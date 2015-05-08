local chuck = require("chuck")
local inspect = require("inspect")
local engine = chuck.engine()
local err,client = chuck.redis.Connect(engine,"127.0.0.1",6379)

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
		client:Query(cmd,gen_callback(query_cb,id))		
	end

	for i = 1,1000 do
		local cmd = string.format("hmget chaid:%d chainfo skills",i)
		client:Query(cmd,gen_callback(query_cb,i))
	end

	chuck.RegTimer(engine,1000,
				   function() 
				   		collectgarbage("collect") 
				   		print(count)
				   		count = 0 
				   end)
	engine:Run()

else
	print(err)
end




