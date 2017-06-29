package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local socket = chuck.socket
local packet = chuck.packet

local event_loop = chuck.event_loop.New()

local timer

local tcpServer

local ip = "127.0.0.1"

local port = 8010

local function server()
	local clients = {}
	local client_count = 0
	local packet_count = 0
	local lastShow = chuck.time.systick()

	tcpServer = socket.stream.ip4.listen(event_loop,ip,port,function (fd,err)
		if err then
			return
		end
		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
			clients[fd] = conn
			client_count = client_count + 1
			conn:Start(event_loop,function (data)
				if data then
					for k,v in pairs(clients) do
						packet_count = packet_count + 1
						--[[
						广播的时候一个套接口可能会连续发送很多包,
						所以这里使用DelaySend尽量合并包一次性发送以提升性能
						(注意这里没有地方调用Flush,所以数据包会在下一次循环检测到套接口可写时执行实际的发送)
						]]--
						v:DelaySend(data)
					end

					local now = chuck.time.systick()
					local delta = now - lastShow
					if delta >= 1000 then
						lastShow = now
						print(string.format("clients:%d,pkt:%.0f/s",client_count,packet_count*1000/delta))
						packet_count = 0
					end

				else
					client_count = client_count - 1
					conn:Close()
					clients[fd] = nil 
				end
			end)
		end
	end)

	if not server then
		return false
	end

	--[[timer = event_loop:AddTimer(1000,function ()
		collectgarbage("collect")
		print(client_count,packet_count)
		packet_count = 0
	end)]]--

	return true	
end


local function client(clientCount)
	for i=1,clientCount do
		socket.stream.ip4.dail(event_loop,ip,port,function (fd,errCode)
			if errCode then
				print("connect error:" .. errCode)
				return
			end
			local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
			if conn then
				conn:Start(event_loop,function (data)
					if data then
						local rpacket = packet.Reader(data)
						local idx = rpacket:ReadI32()
						if idx == fd then
							conn:Send(data)
						end
					else
						conn:Close()
						conn = nil
					end
				end)
				--send the first msg
				local buff = chuck.buffer.New()
				local w = packet.Writer(buff)
				w:WriteI32(fd)
				w:WriteStr("123456789041234i9849018239048174871247189477464712347127489127489217489271498")
				conn:Send(buff)
			end
		end)
	end	
end


local function run()

	local runType = arg[1]
	if not runType then
		print("usage: lua benchmark_brocast both|server|client [clientCount]")
		return
	end
	local clientCount = arg[2]


	if (runType == "both" or runType == "client") and nil == clientCount then
		print("usage: lua benchmark_brocast both|server|client [clientCount]")
		return
	end	

	if runType == "both" or runType == "server" then
		if not server() then
			print("start server failed")
			return
		end
	end

	if runType == "both" or runType == "client" then
		client(clientCount)
	end

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		event_loop:Stop()
	end)
	event_loop:Run()
end

run()
