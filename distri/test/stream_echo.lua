local chuck = require("chuck")
local socket = require("distri.socket")
local engine = require("distri.engine")
local signal = chuck.signal
local signaler = signal.signaler(signal.SIGINT)
local clone     = chuck.packet.clone

local server = socket.stream.listen("127.0.0.1",8010,function (s,errno)
	if s then
		if s:Ok(4096,socket.stream.rawdecoder,function (_,msg,errno)
			if msg then
				s:Send(clone(msg))
			else
				s:Close()
				s = nil
			end
		end) then
			s:SetRecvTimeout(5000)
		end
	end
end)

if server then
	signaler:Register(engine,function ()
		engine:Stop()
	end)
	chuck.RegTimer(engine,1000,function() 
		collectgarbage("collect")	
	end)	
	engine:Run()
end