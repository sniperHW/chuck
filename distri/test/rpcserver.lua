local Task   = require("distri.uthread.task")
local Distri = require("distri.distri")
local Redis  = require("distri.redis")
local Socket = require("distri.socket")
local chuck  = require("chuck")
local RPC    = require("distri.rpc")
local Packet = chuck.packet

local count  = 0

local config = RPC.Config(
function (data)                            --encoder
	local wpk = Packet.wpacket(512)
	wpk:WriteTab(data)
	return wpk
end,
function (packet)                          --decoder
	return packet:ReadTab()
end)

local rpcServer = RPC.Server(config)
rpcServer:RegService("hello",function ()
	return "world"
end)

local server = Socket.stream.Listen("127.0.0.1",8010,function (s,errno)
	if s then
		s:Ok(4096,Socket.stream.decoder.rpacket(4096),function (_,msg,errno)
			if msg then
				rpcServer:ProcessRPC(s,msg)
				count = count + 1
			else
				s:Close()
				s = nil
			end
		end)
	end
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

