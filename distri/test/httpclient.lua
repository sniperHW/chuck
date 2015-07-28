local Http   = require("distri.http.http")
local Distri = require("distri.distri")
local Chuck  = require("chuck")
local Socket = require("distri.socket")
local cjson = require("cjson")

--http://s1.jctt.mmgame.mobi/index.php/ajaxgateway/index/stadium/getUserServerPvp
local c = 0
for i = 1,100 do
	local client1 = Http.Client("s1.jctt.mmgame.mobi")--,8010)
	local function on_response(response,errno)
		if response then
			print(response:Content())
			cjson.decode(response:Content())
			client1:Close()
			c = c + 1
			print("ok",c)
		else
			print("request error")
		end
	end

	client1:Get(Http.Request("/index.php/ajaxgateway/index/stadium/getUserServerPvp"),on_response)
end

local t = Distri.RegTimer(1000,function()
	print("bytecount:" .. Chuck.bytecount())
	collectgarbage("collect")	
end)	

Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
Distri.Run()


