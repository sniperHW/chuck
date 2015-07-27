local Http   = require("distri.http.http")
local Distri = require("distri.distri")
local Chuck  = require("chuck")
local Socket = require("distri.socket")
local cjson = require("cjson")

local c  = 0
local data

for i = 1,100 do
	local client    = Http.Client("www.baidu.com")--"s1.jctt.mmgame.mobi")
	if client then
		local request = Http.Request("/")--"/index.php/ajaxgateway/index/stadium/getUserServerPvp")
		client:Get(request,function (response,errno)
			if response then
				--for k,v in pairs(response:Headers()) do
				--	print(v[1] .. " : " .. v[2])
				--end
				--print("----------------------start--------------------------------")
				--print(response:Content())
				--cjson.decode(response:Content())
				--print(response:Content())
				c = c + 1
				print(c)
				if not data then
					data = response:Content()
				else
					if data == response:Content() then
						print("ok")
					else
						print("error")
					end
				end
				--print(response:Content())
				--print("----------------------end--------------------------------")				
				--print(#response:Content(),i)
			else
				print("request error:" .. errno)
			end
		end)
	end	
end
Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
Distri.Run()


