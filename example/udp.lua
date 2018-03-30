package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local socket = chuck.socket
local packet = chuck.packet

local event_loop = chuck.event_loop.New()

local timer

local udpServer

local ip = "127.0.0.1"

local port = 8010

local function server()
	local clients = {}
	local client_count = 0
	local packet_count = 0
	local bytes = 0
	local lastShow = chuck.time.systick()

	udpServer = socket.datagram.new(socket.AF_INET)
	udpServer:Bind(socket.addr(socket.AF_INET,ip,port))
	udpServer:Start(event_loop,function (data,from,err)
		if data and from then
			packet_count = packet_count + 1
			bytes = bytes + data:Size()
			udpServer:Sendto(data,from)
			local now = chuck.time.systick()
			local delta = now - lastShow
			if delta >= 1000 then
				lastShow = now
				print(string.format("pkt:%.0f/s,bytes:%.2fMB/s",packet_count*1000/delta,(bytes/1024/1024)*1000/delta))
				packet_count = 0
				bytes = 0
				collectgarbage("collect")
			end			
		end
	end)

	if not udpServer then
		return false
	end

	return true	
end


local function client(clientCount)

	local content = ""
	for i = 1,511 do
		content = content .. "11111111"
	end

	local client = socket.datagram.new(socket.AF_INET)
	client:Start(event_loop,function (data,from,err)
		if data and from then
			client:Sendto(data,from)		
		end
	end)
	local buff = chuck.buffer.New(4096)
	local w = packet.Writer(buff)
	w:WriteStr(content)
	client:Sendto(buff,socket.addr(socket.AF_INET,ip,port))		
end


local function run()

	local runType = arg[1]
	if not runType then
		print("usage: lua benchmark_brocast both|server|client")
		return
	end
	local clientCount = arg[2]

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
