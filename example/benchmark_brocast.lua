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

local serverAddr = socket.addr(socket.AF_INET,ip,port)

local function server()
	local clients = {}
	local client_count = 0
	local packet_count = 0
	local lastShow = chuck.time.systick()

	tcpServer = socket.stream.listen(event_loop,serverAddr,function (fd,err)
		if err then
			return
		end
		local conn = socket.stream.socket(fd,4096,packet.Decoder(65536))
		if conn then
			--conn:SetNoDelay(1)
			clients[fd] = conn
			client_count = client_count + 1
			conn:Start(event_loop,function (data)
				if data then
					for k,v in pairs(clients) do
						packet_count = packet_count + 1
						v:Send(data)
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

	return true	
end


local function client(clientCount)
	for i=1,clientCount do
		socket.stream.ip4.dail(event_loop,serverAddr,function (fd,errCode)
			if errCode then
				print("connect error:" .. errCode)
				return
			end
			local conn = socket.stream.socket(fd,4096,packet.Decoder(65536))
			if conn then
				--conn:SetNoDelay(1)
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
				w:WriteStr("1234567890")
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

	event_loop:AddTimer(1000,function ()
		collectgarbage("collect")
	end)

	event_loop:Run()
end

run()
