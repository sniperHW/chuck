package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local http = require("http")
local socket = chuck.socket
local event_loop = chuck.event_loop.New()


--[[
http.Server的适用场景
后面添加线程模块，可以在A线程监听，accept接受到fd之后，将fd分派给工作线程组
由工作线程组调用http.Server(fd)来处理这个连接上的http请求，实现多线程http服务
]]--

--[[local server = socket.stream.ip4.listen(event_loop,"0.0.0.0",8010,function (fd)
	http.Server(event_loop,fd,function (request,response)
		response:SetHeader("Content-Type","text/plain")
		response:SetHeader("A","a")
		response:SetHeader("B","b")
		response:AppendBody("hello everyone")
		response:Finish("200","OK")
	end)
end)]]--

local ret = http.easyServer(function (request,response)
	response:SetHeader("Content-Type","text/plain")
	response:SetHeader("A","a")
	response:SetHeader("B","b")
	response:AppendBody("hello everyone")
	response:Finish("200","OK")
end):Listen(event_loop,"0.0.0.0",8010)

if "OK" == ret then
	local timer1 = event_loop:AddTimer(1000,function ()
		collectgarbage("collect")
	end)
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		print("recv SIGINT stop server")
		event_loop:Stop()
	end)	
	event_loop:Run()
end

