#chuck

* first, Chuck is my son's name.

* second, Chuck is a high perference,asynchronous and easily use C/Lua network library under Linux/Freebsd.

#build

download and make [lua 5.3](http://www.lua.org/)

static library for c:

	make libchuck

dynamic library for lua:

	make chuck.so

c example:

	make samples


#examples

echo server:

	lua src/samples/lua/echoserver.lua

now try telnet to 127.0.0.1 8010.

asynchronous redis client:

start redis server on 127.0.0.1 6379

	lua src/samples/lua/redis_stress.lua 	

httpserver:

	local Http   = require("distri.http.http")
	local Distri = require("distri.distri")
	local Chuck  = require("chuck")

	local server = Http.Server(function (req,res)
		res:WriteHead(200,"OK", {"Content-Type: text/plain"})
	  	res:End("hello world!")
	end):Listen("0.0.0.0",8010)

	if server then
		Distri.Signal(Chuck.signal.SIGINT,Distri.Stop)
		Distri.Run()
	end

httpclient:

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

[more detail](doc/reference.md)

#customer


![](img/20150811163353.jpg "福州市欢乐互动科技有限公司")
