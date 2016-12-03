local chuck = require("chuck")
local redis = chuck.redis
local config = require("config")

local M = {
	redis_conn = nil	
}

M.init = function (event_loop)
   local ip,port = config.redis_ip,config.redis_port
   local stop
   redis.Connect_ip4(event_loop,ip,port,function (conn)
   	M.redis_conn = conn
   	stop = true
   	if not M.redis_conn then
   		print(string.format("connect to redis server %s:%d failed",ip,port))
   	else
   		print("connect to redis ok")
   	end
   end)

   while not stop do
   	event_loop:Run(100)
   end

   return M.redis_conn ~= nil	

end

M.execute = function (cb,cmd,...)
	print("M.execute")
	return M.redis_conn:Execute(cb,cmd,...)
end

return M
