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
	local bytes = 0
	local lastShow = chuck.time.systick()

	tcpServer = socket.stream.listen(event_loop,serverAddr,function (fd,err)
		if err then
			return
		end
		local conn = socket.stream.socket(fd,4096,packet.Decoder(65536))
		if conn then
			clients[fd] = conn
			client_count = client_count + 1

			conn:SetNoDelay(1)
			conn:SetCloseCallBack(function ()
				client_count = client_count - 1
				clients[fd] = nil 
			end)

			conn:Start(event_loop,function (data)
				if data then
					packet_count = packet_count + 1
					bytes = bytes + data:Size()
					conn:Send(data)
					local now = chuck.time.systick()
					local delta = now - lastShow
					if delta >= 1000 then
						lastShow = now
						print(string.format("clients:%d,pkt:%.0f/s,bytes:%.2fMB/s",client_count,packet_count*1000/delta,(bytes/1024/1024)*1000/delta))
						packet_count = 0
						bytes = 0
						collectgarbage("collect")
					end
				else
					conn:Close()
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

	local content = ""
	for i = 1,511 do
		content = content .. "11111111"
	end

	local c = 0
	for i=1,clientCount do
		socket.stream.dial(event_loop,serverAddr,function (fd,errCode)
			if errCode then
				print("connect error:" .. errCode)
				return
			end
			c = c + 1
			local conn = socket.stream.socket(fd,4096,packet.Decoder(65536))
			if conn then
				conn:SetNoDelay(1)
				conn:SetCloseCallBack(function ()
					c = c - 1
					if c == 0 then
						event_loop:Stop()
					end
				end)
				conn:Start(event_loop,function (data)
					if data then
						conn:Send(data)
					else
						conn:Close()
						conn = nil
					end
				end)
				--send the first msg
				local buff = chuck.buffer.New(4096)
				local w = packet.Writer(buff)
				w:WriteStr(content)
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
