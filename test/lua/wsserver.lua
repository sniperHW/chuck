package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local httpserver = require("httpserver").init(event_loop)


local server,ret = httpserver.new(function (request,response)
	local wssocket = httpserver.Upgrade(request,response)
	print(wssocket)
end):Listen("0.0.0.0",8010)

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