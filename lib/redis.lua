local chuck = require("chuck")
local promise
local redis = chuck.redis

local M = {}

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

function M.init(eventLoop)
   if nil == M.eventLoop then
      M.eventLoop = eventLoop
      promise = require("Promise").init(eventLoop)
   end
   return M
end

function M.Command(conn,cmd,...)
	return newRedisCmd(conn,cmd,...)
end

function M.Connect(ip,port,callback)
	return redis.Connect_ip4(M.eventLoop,ip,port,callback)
end

function M.ConnectPromise(ip,port)
   return promise.new(function(resolve,reject)
      local err = redis.Connect_ip4(M.eventLoop,ip,port,function (conn,err)
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