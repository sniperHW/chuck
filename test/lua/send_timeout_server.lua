package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'


local chuck = require("chuck")
local socket = chuck.socket
local event_loop = chuck.event_loop.New()
local log = chuck.log

local server = socket.stream.ip4.listen(event_loop,"127.0.0.1",8010,function (fd)
	local conn = socket.stream.New(fd,4096)
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