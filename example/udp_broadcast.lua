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

	udpServer = socket.datagram.socket(socket.AF_INET)
	udpServer:SetBroadcast()
	if not udpServer then
		return false
	end

	local content = ""
	for i = 1,511 do
		content = content .. "11111111"
	end

	event_loop:AddTimer(1000,function ()
		local buff = chuck.buffer.New(4096)
		local w = packet.Writer(buff)
		w:WriteStr(content)
		print("Broadcast")
		udpServer:Broadcast(buff,socket.addr(socket.AF_INET,"255.255.255.255",port))
	end)	

	return true	
end


local function client(clientCount)

	local client = socket.datagram.socket(socket.AF_INET)
	client:SetBroadcast()
	client:Bind(socket.addr(socket.AF_INET,"255.255.255.255",port))
	client:Start(event_loop,function (data,from,err)
		if data and from then
			print("got Broadcast msg")		
		end
	end)

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
