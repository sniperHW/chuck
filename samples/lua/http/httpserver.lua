local chuck = require("chuck")
local Http   = require("samples.lua.http.http")
local Router = require("samples.lua.http.router")
local signal = chuck.signal

local engine = chuck.engine()

local function sigint_handler()
	print("recv sigint")
	engine:Stop()
end

local signaler = signal.signaler(signal.SIGINT)

Router.RegHandler("/",function (req,res,param)
	res:WriteHead(200,"OK", {"Content-Type: text/plain"})
  	res:End("hello world!")
end)

--ocal server = Http.HttpServer(engine,"127.0.0.1",8010,Router.Dispatch)


--for wrk test
local server = Http.HttpServer(engine,"127.0.0.1",8010,function (req,res)
	res:WriteHead(200,"OK", {"Content-Type: text/plain"})
  	res:End("hello world!")
end)


if server then
	chuck.RegTimer(engine,50,function() 
		collectgarbage("collect")
	end)	
	chuck.RegTimer(engine,1000,function() 
		print(collectgarbage("count")/1024,chuck.buffercount()) 
	end)
	signaler:Register(engine,sigint_handler)
	engine:Run()
end