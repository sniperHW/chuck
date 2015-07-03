local engine = require("distri.engine")
local Sche   = require("distri.uthread.sche")
local chuck  = require("chuck")
local signal = chuck.signal

local distri   = {}
local signaler = {}
local stop

function distri.Run()
	stop = false
	local ms = 1
	while not stop and engine:Run(ms) do
		if Sche.Schedule() > 0 then
			ms = 0
		else
			ms = 5
		end
	end
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
	stop = true
end

return distri
