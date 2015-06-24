local chuck  = require("chuck")
local Http   = require("samples.lua.http.http")
local Router = require("samples.lua.http.router")
local signaler = chuck.signal.signaler(chuck.signal.SIGINT)
local Template = require("samples.lua.http.Template")

signaler:Register(Http.engine,function () 
	print("recv sigint")
	Http.Stop()
end)

Router.RegHandler("/",function (req,res,param)
	res:WriteHead(200,"OK", {"Content-Type: text/plain"})
  	res:End("hello world!")
end)

--local server = Http.HttpServer(Router.Dispatch):Listen("127.0.0.1",8010)

--for wrk test
--[[
local server = Http.HttpServer(function (req,res)
	res:WriteHead(200,"OK", {"Content-Type: text/plain"})
  	res:End("hello world!")
end):Listen("127.0.0.1",8010)
]]--

--dynamic page
local server = Http.HttpServer(function (req,res)
	res:WriteHead(200,"OK", {"Content-Type: text/html"})
  	res:End(Template.Load("samples/lua/http/index.html"))
end):Listen("127.0.0.1",8010)

if server then
	Http.Run()
end