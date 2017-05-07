local chuck = require("chuck")
local redis = chuck.redis

M = {}

local redisCmd = {}
redisCmd.__index = redisCmd

local function newRedisCmd(conn,cmd,...)
	local c = {}
	c.redisConn = conn
	c.cmd = cmd 
	c.args = {...}
	c = setmetatable(c,redisCmd)
	return c
end

function redisCmd:Execute(callback)
	return self.redisConn:Execute(callback,self.cmd,table.unpack(self.args))
end

function M.Command(conn,cmd,...)
	return newRedisCmd(conn,cmd,...)
end

function M.Connect(eventLoop,ip,port,callback)
	return redis.Connect_ip4(eventLoop,ip,port,callback)
end


return M