package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local coroutine = require("ccoroutine")
local CoroSocket = require("CoroSocket").init(event_loop)

local server = CoroSocket.ListenIP4("0.0.0.0",8010,function(socket,err)
	print(socket,err)
	if socket then
		while true do
			local msg,err = socket:RecvUntil('\n',1000)
			if msg then
				socket:Send(msg)
			else
				if err ~= "timeout" then
					socket:Close()
					print("connection lose")
					break
				end
				print("recv timeout")
			end
		end
	end
end)

if server then
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		event_loop:Stop()
	end)
	event_loop:Run()

end


