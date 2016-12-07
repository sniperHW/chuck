package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local socket = chuck.socket
local packet = chuck.packet

local event_loop = chuck.event_loop.New()

local connections = {}
local packet_count = 0
local urgent_count = 0
local total = 0

for i=1,10 do
	socket.stream.ip4.dail(event_loop,"127.0.0.1",8010,function (fd,errCode)
		if 0 ~= errCode then
			print("connect error:" .. errCode)
			return
		end
		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
		connections[fd] = conn
		conn:Start(event_loop,function (data)
				if data then 
					local rpacket = packet.Reader(data)
					local str = rpacket:ReadStr()
					if str == "urgent" then
						urgent_count = urgent_count + 1
					end
					packet_count = packet_count + 1
					total = total + 1
				else
					print("client disconnected") 
					conn:Close()
					connections[fd] = nil 
				end
			end)
		end
	end)
end

local timer1 = event_loop:AddTimer(1000,function ()
	print(packet_count,urgent_count,total)
	collectgarbage("collect")
	packet_count = 0
	urgent_count = 0
end)

local timer2 = event_loop:AddTimer(50,function ()
	for k,v in pairs(connections) do
		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteStr("hellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohello")
		--w:WriteStr("hellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohello")		
		v:Send(buff)
		v:Send(buff)
	end
end)

event_loop:WatchSignal(chuck.signal.SIGINT,function()
	print("recv SIGINT stop client")
	event_loop:Stop()
end)

event_loop:Run()