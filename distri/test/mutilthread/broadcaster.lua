local chuck = require("chuck")
local engine = require("distri.engine")
local Distri = require("distri.distri")
local cthread = chuck.cthread

	
Distri.Signal(chuck.signal.SIGINT,Distri.Stop)
local t = Distri.RegTimer(1000,function() 
	cthread.broadcast({broadcast=1})
	collectgarbage("collect")	
end)
for i = 1,4 do
	cthread.new("distri/test/mutilthread/thread.lua")
end

Distri.Run()
