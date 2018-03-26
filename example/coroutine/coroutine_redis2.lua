package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local coroutine = require("ccoroutine")
local redis = require("redis").init(event_loop)
local promise = require("Promise").init(event_loop)

redis.Connect = coroutine.coroutinize1(redis.Connect)

local count = 0
local lastShow = chuck.time.systick()

coroutine.run(function (self)
	local conn = redis.Connect("127.0.0.1",6579)
	if conn then
		local Execute = coroutine.bindcoroutinize2(conn,conn.Execute)
		--for循环顺序请求，coroutine要经过10次切换
		for i = 1,10 do
			Execute("set","hw" .. i,i)
		end
		local p = {}
		local ret = {}
		for i = 1,11 do
			table.insert(p,redis.ExecutePromise(conn,"get","hw" .. i))
		end

		--通过promise.all同时发出11个操作请求,只需要1次切换
		local current = coroutine.running()
		promise.all(p):andThen(function (results)
			for k,v in pairs(results) do
				if v.state == "resolved" then
					ret[k] = v.value
				else
					ret[k] = nil
				end
			end
			coroutine.resume(current,ret)
		end)

		coroutine.yield()

		for i = 1,11 do
			print("hw" .. i,ret[i])
		end

		--启动11个coroutine并发执行11个请求，利用waitGroup等待结果
		local waitGroup = coroutine.waitGroup()
		for i = 1,11 do
			waitGroup:add()
			coroutine.run(function()
				local result = Execute("get","hw" .. i)
				ret[i] = result
				waitGroup:done()
			end)
		end

		waitGroup:wait()

		for i = 1,11 do
			print("hw" .. i,ret[i])
		end		

		event_loop:Stop()
	end
end)

event_loop:WatchSignal(chuck.signal.SIGINT,function()
	event_loop:Stop()
	print("stop")
end)

event_loop:Run()