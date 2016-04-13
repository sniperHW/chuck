local chuck = require("chuck")
local http = require("http")
local socket = chuck.socket
local event_loop = chuck.event_loop.New()

local server = socket.stream.ip4.listen(event_loop,"0.0.0.0",8010,function (fd)
	local count = 0;
	http.HttpServer(event_loop,fd,function (request,response)
		--count = count + 1
		--print(count)
		--response:SetHeader("Content-Type","text/plain")
		response:AppendBody("hello everyone")
		local ret = response:End("200","OK")
		--if ret then
		--print(ret)
		--end
	end)
end)

local timer1 = event_loop:RegTimer(1000,function ()
	collectgarbage("collect")
end)

if server then
	event_loop:Run()
end