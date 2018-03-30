package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'


local chuck = require("chuck")
local socket = chuck.socket
local event_loop = chuck.event_loop.New()
local log = chuck.log

local serverAddr = socket.Addr(socket.AF_INET,"127.0.0.1",8010)

local server = socket.stream.listen(event_loop,serverAddr,function (fd,err)
	if err then
		return
	end
	local conn = socket.stream.socket(fd,4096)
	if conn then
		--不调用Start,这样就不会从socket接收数据
	end
end)

if server then
	log.SysLog(log.info,"server start")	

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		log.SysLog(log.info,"recv SIGINT stop server")
		event_loop:Stop()
	end)	
	event_loop:Run()
end