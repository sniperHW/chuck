package.cpath = './lib/?.so;'
package.path = './lib/?.lua;'

local chuck = require("chuck")
--local redis = require("redis")
local redis = chuck.redis
local promise = require("promise")
local event_loop = chuck.event_loop.New()

local function Connect(ip,port)
   return promise.new(function(resolve,reject)
      local err = redis.Connect_ip4(event_loop,ip,port,function (conn,err)
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

local function Execute(conn,cmd,...)
   local args = {...}
   return promise.new(function (resolve,reject)
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

local redis_conn

Connect("127.0.0.1",8010):andThen(function (conn)
   print("connect to redis ok")
   redis_conn = conn
   return Execute(redis_conn,"set","sniper","hw")
end):andThen(function (conn)
   return Execute(redis_conn,"get","sniper")
end):andThen(function (reply)
   print(reply)
   return Execute(redis_conn,"ping")
end):andThen(function (reply)
   print(reply)
end):catch(function (err)
   print("connect to redis error:" .. err)
end)


event_loop:Run()

