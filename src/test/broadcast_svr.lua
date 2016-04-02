local chuck = require("chuck")
local socket = chuck.socket
local packet = chuck.packet

local event_loop = chuck.event_loop.New()

local clients = {}
local client_count = 0
local packet_count = 0

local server = socket.stream.ip4.listen(event_loop,"127.0.0.1",8010,function (fd)
	local conn = socket.stream.New(fd,65536,packet.Decoder())
	if conn then
		clients[fd] = conn
		client_count = client_count + 1
		conn:Bind(event_loop,function (data)
			if data then 
				for k,v in pairs(clients) do
					packet_count = packet_count + 1
					v:Send(data)
				end
			else
				client_count = client_count - 1
				print("client disconnected") 
				conn:Close()
				clients[fd] = nil 
			end
		end)
	end
end)

local timer1 = event_loop:RegTimer(1000,function ()
	collectgarbage("collect")
	print(client_count,packet_count)
	packet_count = 0
end)

if server then
	event_loop:Run()
end