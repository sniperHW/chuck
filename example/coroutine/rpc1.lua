package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local packet = chuck.packet
local event_loop = chuck.event_loop.New()
local rpc = require("rpc").init(event_loop)
local coroutine = require("ccoroutine")


local pool_client = coroutine.pool(0,100)

local count = 0
local lastShow = chuck.time.systick()

rpc.registerMethod("hello",function (response,a,b)
	response:Return(a .. " " .. b,"sniperHW hahaha")
	count = count + 1
end)

rpc.pack = function(rpcmsg)
	local buff = buffer.New()
	local writer = packet.Writer(buff)
	writer:WriteStr(rpcmsg)
	return buff
end


--同步调用，只能在coroutine上下文中调用，接到response之后才唤醒阻塞的coroutine
local function SyncCall(client,func,...)
	local current = coroutine.running()
	if not current then
		return error("SyncCall must be call in coroutine context")
	end
	local err = client:Call(func,function (...)
		coroutine.resume(current, ...)
	end,...)
	if err then
		return err
	else
		return coroutine.yield()
	end
end

local function AsynCall(client,func,...)
	return client:Call(func,nil,...)
end

local timer

local serverAddr = socket.addr(socket.AF_INET,"127.0.0.1",8010)

local function main()

	local server = socket.stream.listen(event_loop,serverAddr,function (fd,err)
		if err then
			return
		end
		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
			conn:Start(event_loop,function (data)
				if stop then
					conn:Close()
					return
				end
				if data then
					local reader = packet.Reader(data)
					rpc.OnRPCMsg(conn,reader:ReadStr())
				else
					conn:Close()
				end
			end)
		end
	end)

	socket.stream.dail(event_loop,serverAddr,function (fd,errCode)
		if errCode then
			print("connect error:" .. errCode)
			return
		end
		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
			conn:Start(event_loop,function (data)
				if data then 
					local reader = packet.Reader(data)
					rpc.OnRPCMsg(conn,reader:ReadStr())
				else 
					conn:Close()
					rpc.OnConnClose(conn) 
				end
			end)
			local rpcClient = rpc.RPCClient(conn)
			for i = 1,10 do
				pool_client:addTask(function ()
					while true do
						local err,result = SyncCall(rpcClient,"hello","hello","world")
						if err then
							break
						end
					end
				end)
			end
		end
	end)
	timer = event_loop:AddTimer(1000,function ()
		local now = chuck.time.systick()
		local delta = now - lastShow
		lastShow = now
		print(string.format("count:%.0f/s elapse:%d",count*1000/delta,delta))
		count = 0
	end)

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		event_loop:Stop()
	end)

	event_loop:Run()
end

main()


