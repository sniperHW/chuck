local chuck  = require("chuck")
local signaler = chuck.signal.signaler(chuck.signal.SIGINT)
local Http   = require("samples.lua.http.http")

signaler:Register(Http.engine,function () 
	print("recv sigint")
	Http.Stop()
end)

local client    = Http.HttpClient("www.baidu.com")
if client then
	local request = Http.HttpRequest("/")
	client:Get(request,function (response)
		for k,v in pairs(response:Headers()) do
			print(v[1] .. " : " .. v[2])
		end
		print(response:Content())
	end)
	Http.Run() 
end