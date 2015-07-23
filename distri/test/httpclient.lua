local Http   = require("distri.http.http")
local Distri = require("distri.distri")
local Chuck  = require("chuck")

local client    = Http.Client("www.baidu.com")
if client then
	local request = Http.Request("/")
	client:Get(request,function (response)
		for k,v in pairs(response:Headers()) do
			print(v[1] .. " : " .. v[2])
		end
		print(response:Content())
	end)
	Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
	Distri.Run()	
end


