package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local socket = chuck.socket
local packet = chuck.packet
local ssl = chuck.ssl


local event_loop = chuck.event_loop.New()

local serverAddr = socket.addr(socket.AF_INET,"127.0.0.1",8010)

socket.stream.dial(event_loop,serverAddr,function (fd,errCode)
	if errCode then
		print("connect error:" .. errCode)
		return
	end
	local conn = socket.stream.socket(fd,4096,packet.Decoder(65536))
	if conn then

		local err = ssl.SSL_connect(conn)

		if err then
			print("SSL_connect error")
			conn:Close()
			return
		end

		print("connect ok")

		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteStr("hello")
		conn:Send(buff)

		conn:Start(event_loop,function (data)
			if data then 
				print("got response",data:Content())
				conn:Close()
			else
				print("client disconnected") 
				conn:Close()
			end
		end)

	end
end)

event_loop:WatchSignal(chuck.signal.SIGINT,function()
	print("recv SIGINT stop client")
	event_loop:Stop()
end)

event_loop:Run()