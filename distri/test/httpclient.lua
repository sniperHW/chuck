local Http   = require("distri.http.http")
local Distri = require("distri.distri")
local Chuck  = require("chuck")
local Socket = require("distri.socket")

local client    = Http.Client("www.baidu.com")
if client then
	local request = Http.Request("/")
	client:Get(request,function (response,errno)
		if response then
			for k,v in pairs(response:Headers()) do
				print(v[1] .. " : " .. v[2])
			end
			print(response:Content())
		else
			print("request error:" .. errno)
		end
	end)
	Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
	Distri.Run()	
end


