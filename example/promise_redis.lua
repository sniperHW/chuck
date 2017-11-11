package.cpath = './lib/?.so;'
package.path = './lib/?.lua;'

local chuck = require("chuck")
local redis = require("redis")
local event_loop = chuck.event_loop.New()
local promise = require("Promise")

local redis_conn

redis.ConnectPromise(event_loop,"127.0.0.1",6379):andThen(function (conn)
   print("connect to redis ok")
   redis_conn = conn
   return redis.ExecutePromise(redis_conn,"zrange","rank",0,4)
end):andThen(function (reply)
   --print(#reply)
   for k,v in pairs(reply) do
   	print(v)
   end
   --return redis.ExecutePromise(redis_conn,"get","sniper")
end)--[[:andThen(function (reply)
   print(reply)
   return redis.ExecutePromise(redis_conn,"ping")
end):andThen(function (reply)
   print(reply)
end)]]:catch(function (err)
   print("connect to redis error:" .. err)
end)


event_loop:Run()

