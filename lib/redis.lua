local chuck = require("chuck")
local promise = require("Promise")
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

function M.ConnectPromise(eventLoop,ip,port)
   return promise.new(function(resolve,reject)
      local err = redis.Connect_ip4(eventLoop,ip,port,function (conn,err)
            if err then
               reject(err)
            else
               resolve(conn)
            end
         end)
      if err then
         reject(err)
      end
   end)	
end

function M.ExecutePromise(conn,cmd,...)
	local args = {...}
   	return promise.new(function (resolve,reject)
   		if nil == conn then
   			return reject("connection closed")
   		end
      	local err = conn:Execute(function (reply,err)
            if err then
               reject(err)
            else
               resolve(reply)
            end
         end,cmd,table.unpack(args))
      	if err then
         	reject(err)
      	end
   	end)	
end

return M