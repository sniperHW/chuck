package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local event_loop = chuck.event_loop.New()
local log = chuck.log


local serverAddr = socket.addr(socket.AF_INET,"127.0.0.1",9010)

local server = socket.stream.listen(event_loop,serverAddr,function (fd)
	local conn = socket.stream.socket(fd,4096)
	if conn then

		local peerAddr = conn:GetPeerAddr()

		print("client from",socket.util.inet_ntop(peerAddr),socket.util.inet_port(peerAddr))

		conn:Start(event_loop,function (data,err)
			if data then
				local response = data:Clone()
				response:AppendStr("hello world\r\n")
				conn:Send(response)
			else
				log.SysLog(log.info,string.format("client disconnected:%s",err)) 
				conn:Close() 
			end
		end)
	end
end)

print("systick:",chuck.time.systick(),"unixtime:",chuck.time.unixtime())

if server then
	log.SysLog(log.info,"server start")	
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		log.SysLog(log.info,"recv SIGINT stop server")
		event_loop:Stop()
	end)	
	event_loop:Run()
end


