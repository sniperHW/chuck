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

	tcpServer = socket.stream.listen(event_loop,serverAddr,function (fd,err)
		if err then
			return
		end
		local conn = socket.stream.socket(fd,4096,packet.Decoder(65536))
		if conn then
			conn:Start(event_loop,function (data,err)
				if data then
					local reader = packet.Reader(data)
					print(reader:ReadStr())
				else
					if err == 44 then
						local buff = chuck.buffer.New(4096)
						local w = packet.Writer(buff)
						w:WriteStr("bye bye!")
						conn:Send(buff)
						conn:Close(1)
					else
						conn:Close()
					end
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
	socket.stream.dail(event_loop,serverAddr,function (fd,errCode)
		if errCode then
			print("connect error:" .. errCode)
			return
		end

		local conn = socket.stream.socket(fd,4096,packet.Decoder(65536))
		if conn then
			conn:Start(event_loop,function (data,err)
				if data then
					local reader = packet.Reader(data)
					print(reader:ReadStr())
				else
					print("client:" .. err)
					conn:Close()
				end
			end)
			--send the first msg
			local buff = chuck.buffer.New(4096)
			local w = packet.Writer(buff)
			w:WriteStr("hello")
			conn:Send(buff)
			conn:ShutDownWrite()
		end
	end)
end


local function run()

	if not server() then
		print("start server failed")
		return
	end

	client(clientCount)
	
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		event_loop:Stop()
	end)
	event_loop:Run()
end

run()
