local Task   = require("distri.uthread.task")
local Distri = require("distri.distri")
local Redis  = require("distri.redis")
local Socket = require("distri.socket")
local chuck  = require("chuck")
local RPC    = require("distri.rpc")
local Sche   = require("distri.uthread.sche")
local Config = require("distri.test.rpcconfig")

local rpcClient = RPC.Client(Config)

Task.New(function ()
	local err,s = Socket.stream.Connect("127.0.0.1",8010)
	local function on_msg(_,msg,errno)
		if msg then
			RPC.OnRPCResponse(Config,s,msg)
		else
			print("close")
			s:Close()
			s = nil
		end		
	end
	if s and s:Ok(65535,Socket.stream.decoder.rpacket(4096),on_msg) then
		rpcClient:Connect(s)
		local main = function ()
			while true do
				local err,ret = rpcClient:Call("hello")
				if ret ~= "world" then
					print(err)
					break
				end
			end	
		end		
		for i = 1,5000 do Sche.Spawn(main) end
	end
end)

Distri.Signal(chuck.signal.SIGINT,Distri.Stop)
Distri.Run()