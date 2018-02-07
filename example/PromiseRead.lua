package.cpath = './lib/?.so;'
package.path = './lib/?.lua;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local PromiseConnection = require("PromiseConnection").init(event_loop)

local server = PromiseConnection.listen("127.0.0.1",9010,function (conn)

	print("newclient",conn)

	conn:SetCloseCallBack(function (err)
		print("client disconnected:",err)
	end)

	conn:Send("hello\n")

	local function recv()
		conn:RecvUntil("\r\n"):andThen(function (msg) 
			conn:Send(msg)
			return recv()
		end):catch(function (err)
			print(err)
		end)
	end
	recv()
end)

if server then
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		event_loop:Stop()
	end)	
	event_loop:Run()
end