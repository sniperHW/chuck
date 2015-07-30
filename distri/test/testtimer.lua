local Distri = require("distri.distri")
local Chuck  = require("chuck")

function regT()
	local t
	t = Distri.RegTimer(1,function()
		t:UnRegister()
		t = nil
		regT()	
	end)
end

regT()

local t = Distri.RegTimer(1000,function()
	print("collect")
	collectgarbage("collect")	
end)

Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
Distri.Run()