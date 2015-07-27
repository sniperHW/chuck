local Http   = require("distri.http.http")
local Distri = require("distri.distri")
local Chuck  = require("chuck")
local Socket = require("distri.socket")

local client    = Http.Client("s1.jctt.mmgame.mobi")
if client then
	for i = 1,100 do
		local request = Http.Request("/index.php/ajaxgateway/index/stadium/getUserServerPvp")
		client:Post(request,function (response,errno)
			if response then
				for k,v in pairs(response:Headers()) do
					print(v[1] .. " : " .. v[2])
				end
				--print(response:Content())
				print(#response:Content(),i)
			else
				print("request error:" .. errno)
			end
		end)
	end
	Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
	Distri.Run()	
end


