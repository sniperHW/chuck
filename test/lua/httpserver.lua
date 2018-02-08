package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local http = require("http").init(event_loop)
local socket = chuck.socket


local server,ret = http.Server(function (request,response)
	response:SetHeader("Content-Type","text/plain")
	response:SetHeader("A","a")
	response:SetHeader("B","b")
	response:AppendBody("hello everyone")
	response:Finish("200","OK")
		http.Upgrade(request,response)
end):Listen("127.0.0.1",8010)

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

