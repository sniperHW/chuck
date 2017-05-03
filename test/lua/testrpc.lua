package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local rpc = require("rpc")
local event_loop = chuck.event_loop.New()

rpc.registerMethod("hello",function (response,a,b)
	response:Return(a .. " " .. b,"sniperHW hahaha")
end)


local function main()
	local server = rpc.startServer(event_loop,"127.0.0.1",8010)
	if not server then
		print("start rpc server failed")
		return
	end

	local rpcCallBack = function (err,result)
		if not err then
			print(table.unpack(result))
		else
			print(err)
		end
	end 

	local timer
	
	rpc.connectServer(event_loop,"127.0.0.1",8010,function (rpcClient,err)
		if rpcClient then
			timer = event_loop:AddTimer(1000,function ()
				rpcClient:Call("hello",rpcCallBack,"hello","world")
				rpcClient:Call("world",rpcCallBack,"hello","world")				
			end)
		end
	end)


	event_loop:Run()

end

main()

