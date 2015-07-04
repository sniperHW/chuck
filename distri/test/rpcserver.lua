local Task   = require("distri.uthread.task")
local Distri = require("distri.distri")
local Redis  = require("distri.redis")
local Socket = require("distri.socket")
local chuck  = require("chuck")
local RPC    = require("distri.rpc")
local Config = require("distri.test.rpcconfig")

local count  = 0

local rpcServer = RPC.Server(Config)
rpcServer:RegService("hello",function ()
	return "world"
end)

local server = Socket.stream.Listen("127.0.0.1",8010,function (s,errno)
	if not s then 
		return
	end
	s:Ok(65535,Socket.stream.decoder.rpacket(4096),function (_,msg,errno)
		if msg then
			rpcServer:ProcessRPC(s,msg)
			count = count + 1
		else
			s:Close(errno)
			s = nil
		end
	end)
end)

if server then
	local last = chuck.systick()
	Distri.RegTimer(1000,function ()
   		collectgarbage("collect") 
   		local now = chuck.systick()
   		print("rpc call:" .. count*1000/(now-last) .. "/s")
   		last = now
   		count = 0 
	end)
	Distri.Signal(chuck.signal.SIGINT,Distri.Stop)
	Distri.Run()
end

