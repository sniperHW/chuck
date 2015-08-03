local chuck = require("chuck")
local engine = chuck.engine()
local err,client = chuck.redis.Connect(engine,"127.0.0.1",6379)
local signal = chuck.signal

local function sigint_handler()
	print("recv sigint")
	engine:Stop()
end

local signaler = signal.signaler(signal.SIGINT)

if client then
	local count = 0
	local request = 0
	local response = 0
	local max = 1000000

	local function gen_callback(cb,ud)
		return function (conn,reply)
			cb(conn,reply,ud)
		end
	end

	local function pop_cb(conn,reply,id)
		count = count + 1
		response = response + 1
		if response < max then
			request = request + 2
			local cmd = string.format("LPUSH list %d",reply)
			client:Execute(cmd,function ()
				count = count + 1
				response = response + 1			
			end)
			cmd = string.format("lpop list")
			client:Execute(cmd,gen_callback(pop_cb))
		end					
	end

	local function incr_cb(conn,reply,id)
		count = count + 1
		response = response + 1
		if response < max then
			request = request + 1				
			local cmd = string.format("INCR counter")
		    client:Execute(cmd,gen_callback(incr_cb))
		end
	end

	local function query_cb(conn,reply,id)
		count = count + 1
		response = response + 1
		if response < max then
			request = request + 1					
			local cmd = string.format("hmget chaid:%d chainfo skills",id)
		    client:Execute(cmd,gen_callback(query_cb,id))
	  	end	
	end
	
	client:Execute("set counter 1")

	for i = 1,1000 do
		local cmd = string.format("hmset chaid:%d chainfo %s skills %s",i,
								  "fasdfasfasdfasdfasdfasfasdfasfasdfdsaf",
								  "fasfasdfasdfasfasdfasdfasdfcvavasdfasdf")
		client:Execute(cmd)
	end

	for i = 1,1000 do
		request = request + 1
		local cmd = string.format("LPUSH list %d",i)
		client:Execute(cmd,function ()
			count = count + 1
			response = response + 1			
		end)
	end

	for i = 1,2 do
		request = request + 1
		local cmd = string.format("lpop list")
		client:Execute(cmd,gen_callback(pop_cb))
	end

	for i = 1,2 do
		request = request + 1
		local cmd = string.format("hmget chaid:%d chainfo skills",i)
	    client:Execute(cmd,gen_callback(query_cb,i))	
	end

	request = request + 1
	client:Execute("incr counter",incr_cb)

	local last = chuck.systick()
	local t = chuck.RegTimer(engine,1000,
				   function() 
				   		collectgarbage("collect") 
				   		local now = chuck.systick()
				   		print("hmget:" .. count*1000/(now-last) .. "/s" .. " request:" .. request .. " response:" .. response)
				   		last = now
				   		count = 0 
				   end)
	signaler:Register(engine,sigint_handler)
	engine:Run()

else
	print(err)
end




