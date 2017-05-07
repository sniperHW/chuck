local chuck = require("chuck")
local redis = chuck.redis
local config = require("config")
local log = chuck.log
local logger = log.CreateLogfile("DB")

local M = {
	redis_conn = nil,
   eventLoop  = nil,
   timer      = nil,
   delayReconnect = 1000,	
}

local OnConnectionLoss = nil
local reConnect = nil

reConnect = function (delay,on_init_ok)
   M.timer = M.eventLoop:AddTimer(delay,function ()
      local ip,port = config.redis_ip,config.redis_port
      local err = redis.Connect_ip4(M.eventLoop,ip,port,function (conn)
         if conn then
            M.redis_conn = conn
            M.redis_conn:OnConnectionLoss(OnConnectionLoss)
            if on_init_ok then
               on_init_ok()
            end
         else
            logger:Log(log.error,string.format("connect to db server %s:%d failed",ip,port))
            reConnect(M.delayReconnect)
         end
      end)

      if err then
         logger:Log(log.error,string.format("connect to db server %s:%d error:%s",ip,port,err))
         reConnect(M.delayReconnect)
      end
      M.timer = nil       
      return -1
   end)
end

OnConnectionLoss = function ()
   logger:Log(log.error,string.format("db server connection %s:%d losed",config.redis_ip,config.redis_port))
   M.redis_conn = nil
   reConnect(M.delayReconnect)      
end

M.init = function (eventLoop,on_init_ok)
   if M.eventLoop then
      return "init must be call once"
   end
   M.eventLoop = eventLoop
   reConnect(0,on_init_ok)
end

M.execute = function (cb,cmd,...)
   if M.redis_conn == nil then
      return "not connected to server"
   end
	
   return M.redis_conn:Execute(cb,cmd,...)
end

return M
