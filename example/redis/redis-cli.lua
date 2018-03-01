package.cpath = 'lib/?.so;'
package.path = 'lib/?.lua;'

local chuck = require("chuck")
local readline = require("readline")
local event_loop = chuck.event_loop.New()
local redis = require("redis").init(event_loop)
local redis_conn


function redis_execute(cmd,...)
	local result = nil
	local execute_return = false
	local ret = redis.Command(redis_conn,cmd,...):Execute(function(reply,err)
		if not err then
			print("Execute ok")
		else
			print("Execute error:" .. err)
			redis_conn:Close()
			redis_conn = nil
			return
		end
		result = reply
		execute_return = true
	end)
	
	if not ret then
		while not execute_return do
			event_loop:Run(100)
		end

		return result
	else
		print("Execute error:" .. ret)
		return nil
	end
end

local function execute_chunk(str)
	local func,err = load(str)
	if func then
		local ret,err = pcall(func)
		if not ret then
			print("command error:" .. err)
		end
	elseif err then
		print("command error:" .. err)
	end
end

local function repl()
	local chunk = ""

	local prompt = ">>"

	while true do
		local cmd_line = readline(prompt)
		if #cmd_line > 1 then
			if string.byte(cmd_line,#cmd_line) ~= 92 then
				chunk = chunk .. cmd_line
				break
			else
			  	chunk = chunk .. string.sub(cmd_line,1,#cmd_line-1) .. "\n"
				prompt = ">>>"
			end
		else
			break
		end	
	end

	if chunk ~= "" then
		if chunk == "exit" then
			redis_conn = nil
		else
			execute_chunk(chunk)
		end
	end
end


if arg == nil or #arg ~= 2 then
	print("useage:lua redis-cli.lua ip port")
else
   local ip,port = arg[1],arg[2]
   local stop
   redis.Connect(ip,port,function (conn)
   	redis_conn = conn
   	stop = true
   	if not redis_conn then
   		print(string.format("connect to redis server %s:%d failed",ip,port))
   	else
   		print("hello to redis-cli.lua! use \\ to sperate mutil line!use exit to terminate!")
   	end
   end)

   while not stop do
   	event_loop:Run(100)
   end

   if redis_conn then
	   redis_conn:OnConnectionLoss(function ()
	   	print("connection loss")
	   	redis_conn = nil
	   end)
   end

   while redis_conn do
   		repl()	
   end
end	


--[[
test
redis_execute("hmset","chaid:1","chainfo","fasdfasfasdfasdfasdfasfasdfasfasdfdsaf","skills","fasfasdfasdfasfasdfasdfasdfcvavasdfasdf")\
result = redis_execute("hmget","chaid:1","chainfo","skills")\
print(result[1],result[2])
]]--
--json = require("cjson")
--str = json.encode({gameserver={ip="127.0.0.1",port=8011},roomserver={{ip="127.0.0.1",port=8012}}})

--redis_execute("set","server_config",[[{gameserver={ip="127.0.0.1",port="8011"},gateserver=}]])

--cas print(redis_execute("eval","if not redis.call('get',KEYS[1]) then redis.call('set',KEYS[1],ARGV[1]) end return ARGV[1]","1","foo2","boo2"))