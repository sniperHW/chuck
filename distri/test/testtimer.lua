local Distri = require("distri.distri")
local Chuck  = require("chuck")

function regT()
	local t = Distri.RegTimer(1,function()
		t:UnRegister()
		t = nil
		regT()	
	end)
end

local t = Distri.RegTimer(1000,function()
	collectgarbage("collect")	
end)

Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
Distri.Run()