
package.cpath = './lib/?.so;'
package.path = './lib/?.lua;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local redis = require("redis").init(event_loop)
local promise = require("Promise").init(event_loop)

local redis_conn

redis.ConnectPromise("127.0.0.1",6379):andThen(function (conn)
   print("connect to redis ok")
   redis_conn = conn
   return redis.ExecutePromise(redis_conn,"set","sniper","hw")
end):andThen(function (conn)
   return redis.ExecutePromise(redis_conn,"get","sniper")
end):andThen(function (reply)
   print(reply)
   return redis.ExecutePromise(redis_conn,"ping")
end):andThen(function (reply)
   print(reply)
   return "ok"
end):catch(function (err)
   print("connect to redis error:" .. err)
   return "connect error:" .. err
end):final(function (status)
   print(status)
   event_loop:Stop()
end)


event_loop:Run()