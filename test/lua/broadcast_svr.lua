package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local socket = chuck.socket
local packet = chuck.packet

local event_loop = chuck.event_loop.New()

local clients = {}
local client_count = 0
local packet_count = 0

local server = socket.stream.ip4.listen(event_loop,"127.0.0.1",8010,function (fd)
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
			else
				client_count = client_count - 1
				print("client disconnected") 
				conn:Close()
				clients[fd] = nil 
			end
		end)
	end
end)

local timer1 = event_loop:AddTimer(1000,function ()
	collectgarbage("collect")
	print(client_count,packet_count)
	packet_count = 0
end)

local timer2 = event_loop:AddTimer(5,function ()
	--发送紧急包
	for k,v in pairs(clients) do
		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteStr("urgent")		
		v:SendUrgent(buff)
	end
end)

print("server run")

if server then
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		print("recv SIGINT stop server")
		event_loop:Stop()
	end)
	event_loop:Run()
end