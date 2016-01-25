local chuck = require("chuck")
local socket = chuck.socket
local packet = require("packet")

local event_loop = chuck.event_loop.New()

local connections = {}
local packet_count = 0

for i=1,500 do
	socket.stream.ip4.dail(event_loop,"127.0.0.1",8010,function (fd,errCode)
		if 0 ~= errCode then
			print("connect error:" .. errCode)
			return
		end
		local conn = socket.stream.New(fd,65536,packet.Decoder())
		if conn then
		connections[fd] = conn
		conn:Bind(event_loop,function (data)
				if data then 
					packet_count = packet_count + 1
				else
					print("client disconnected") 
					conn:Close()
					connections[fd] = nil 
				end
			end)
		end
	end)
end

local timer1 = event_loop:RegTimer(1000,function ()
	print(packet_count)
	collectgarbage("collect")
	packet_count = 0
end)

local timer2 = event_loop:RegTimer(300,function ()
	for k,v in pairs(connections) do
		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteStr("hello")
		v:Send(buff)
	end
end)

event_loop:Run()
