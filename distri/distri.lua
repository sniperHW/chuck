local engine = require("distri.engine")
local Sche   = require("distri.uthread.sche")
local chuck  = require("chuck")
local signal = chuck.signal

local distri   = {}
local signaler = {}

function distri.Run()
	chuck.RegTimer(engine,1,function()
		Sche.Schedule()
	end)
	engine:Run()
end

function distri.RegTimer(ms,cb)
	return chuck.RegTimer(engine,ms,cb)
end

function distri.Signal(sig,handler)
	local s = signaler[sig]
	if not s then
		s = {signal.signaler(sig),handler}
		s[1]:Register(engine,function (signo)
			s[2](signo)
		end)
		signaler[sig] = s
	end
	s[2] = handler
end

function distri.Stop()
	engine:Stop()
end

return distri
