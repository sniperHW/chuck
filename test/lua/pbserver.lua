package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local protobuf = require "protobuf"
local chuck = require("chuck")
local socket = chuck.socket
local event_loop = chuck.event_loop.New()
local log = chuck.log

local addr = io.open("test/lua/addressbook.pb","rb")
local buffer = addr:read "*a"
addr:close()

protobuf.register(buffer)


local server = socket.stream.ip4.listen(event_loop,"127.0.0.1",8010,function (fd)
	local conn = socket.stream.New(fd,4096)
	if conn then
		conn:Start(event_loop,function (data)
			if data then
				local decode = protobuf.decode("tutorial.Person" , data:Content())
				print("--------------------Person------------------------")
				print("name:" .. decode.name .. " id:" .. decode.id)
				for k,v in pairs(decode.phone) do
					print("number:" .. v.number .. " " .. v.type or "")
				end
				conn:Close()
			else
				log.SysLog(log.info,"client disconnected") 
				conn:Close() 
			end
		end)
	end
end)

if server then
	log.SysLog(log.info,"server start")	

	local timer1 = event_loop:AddTimer(1000,function ()
		collectgarbage("collect")
	end)

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		log.SysLog(log.info,"recv SIGINT stop server")
		event_loop:Stop()
	end)	
	event_loop:Run()
end