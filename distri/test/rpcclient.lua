local Task   = require("distri.uthread.task")
local Distri = require("distri.distri")
local Redis  = require("distri.redis")
local Socket = require("distri.socket")
local chuck  = require("chuck")
local RPC    = require("distri.rpc")
local Packet = chuck.packet

local config = RPC.Config(
function (data)
	local wpk = Packet.wpacket(512)
	wpk:WriteTab(data)
	return wpk
end,
function (packet)
	return packet:ReadTab()
end)

local rpcClient = RPC.Client(config)


if Socket.stream.Connect("127.0.0.1",8010,function (s,errno)
	if s then
		if s:Ok(4096,Socket.stream.decoder.rpacket(4096),function (_,msg,errno)
			if msg then
				RPC.OnRpcResponse(config,s,msg)
			else
				s:Close()
				s = nil
			end
		end) then
			rpcClient:Connect(s)
			for i = 1,100 do
				Task.New(function ()
					while true do
						local err,ret = rpcClient:Call("hello")
						if err then
							print("err")
							break
						end
					end
				end)
			end
		end
	end
end) then
	Distri.Run()
end