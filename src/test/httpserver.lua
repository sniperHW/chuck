package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local http = require("http")
local socket = chuck.socket
local event_loop = chuck.event_loop.New()

local server = socket.stream.ip4.listen(event_loop,"0.0.0.0",8010,function (fd)
	http.HttpServer(event_loop,fd,function (request,response)
		response:SetHeader("Content-Type","text/plain")
		response:SetHeader("A","a")
		response:SetHeader("B","b")
		response:AppendBody("hello everyone")
		response:End("200","OK")
	end)
end)

local timer1 = event_loop:RegTimer(1000,function ()
	collectgarbage("collect")
end)

if server then
	event_loop:Run()
end
