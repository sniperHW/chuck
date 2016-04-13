local chuck = require("chuck")
local http = require("http")
local socket = chuck.socket
local event_loop = chuck.event_loop.New()

socket.stream.ip4.dail(event_loop,"127.0.0.1",8010,function (fd,errCode)
	if 0 ~= errCode then
		print("connect error:" .. errCode)
		return
	end
	local httpclient = http.HttpClient(event_loop,"127.0.0.1",fd)
	--local c = 0
	local OnResponse
	OnResponse = function (response)
		--c = c + 1
		print("got response",c)
		httpclient:Get("/",nil,OnResponse)
	end

	httpclient:Get("/",nil,OnResponse)
end)


event_loop:Run()