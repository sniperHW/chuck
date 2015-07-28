local Http   = require("distri.http.http")
local Distri = require("distri.distri")
local Chuck  = require("chuck")
local Socket = require("distri.socket")
local cjson = require("cjson")

local c  = 0
local function on_response(response,errno)
	if response then
		c = c + 1
		print(c)
		--local client = Http.Client("www.baidu.com")
		--client:Get(Http.Request("/"),on_response)
	else
		print("request error")
	end
end

local client1 = Http.Client("www.baidu.com")
client1:Get(Http.Request("/"),on_response)

local t = Distri.RegTimer(1000,function()
	collectgarbage("collect")	
end)	

Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
Distri.Run()


