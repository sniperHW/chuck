local Http   = require("distri.http.http")
local Distri = require("distri.distri")
local Chuck  = require("chuck")

local server = Http.Server(function (req,res)
	res:WriteHead(200,"OK", {"Content-Type: text/plain"})
  	res:End("hello world!")
end):Listen("0.0.0.0",8010)

if server then
	local t = Distri.RegTimer(1000,function()
		collectgarbage("collect")	
	end)		
	Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
	Distri.Run()
end