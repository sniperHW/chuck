package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

--[[
	在同一个连接上传递普通消息与RPC调用信息
]]--

local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local packet = chuck.packet
local rpc = require("rpc")
local event_loop = chuck.event_loop.New()

rpc.registerMethod("hello",function (response,a,b)
	response:Return(a .. " " .. b,"sniperHW hahaha")
end)

local cmd_rpc = 100

local c = 0

rpc.pack = function(rpcmsg)
	local buff = buffer.New()
	local writer = packet.Writer(buff)
	writer:WriteI32(cmd_rpc)
	writer:WriteStr(rpcmsg)
	return buff
end

local function main()

	local server = socket.stream.ip4.listen(event_loop,"127.0.0.1",8010,function (fd)
		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
			conn:Start(event_loop,function (data)
				if data then
					local reader = packet.Reader(data)
					local cmd = reader:ReadI32()
					if cmd == cmd_rpc then
						--处理RPC调用
						c = c + 1
						if c == 11 then
							conn:Close()
							return
						end
						rpc.OnRPCMsg(conn,reader:ReadStr())
					else
						--处理普通消息
						print("normal msg:" .. cmd)
					end 
				else
					conn:Close()
				end
			end)
		end
	end)

	local rpcCallBack = function (err,result)
		if not err then
			print(table.unpack(result))
		else
			print(err)
		end
	end 

	local timer

	socket.stream.ip4.dail(event_loop,"127.0.0.1",8010,function (fd,errCode)
		if 0 ~= errCode then
			print("connect error:" .. errCode)
			return
		end
		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
			conn:Start(event_loop,function (data)
				if data then 
					local reader = packet.Reader(data)
					local cmd = reader:ReadI32()
					if cmd == cmd_rpc then
						rpc.OnRPCMsg(conn,reader:ReadStr())
					else
						print("invaild msg")
					end 
				else 
					conn:Close()
					rpc.OnConnClose(conn) 
				end
			end)

			local rpcClient = rpc.RPCClient(conn)
			timer = event_loop:AddTimer(1000,function ()
				--发起RPC调用
				local err = rpcClient:Call("hello",rpcCallBack,"hello","world")
				if err then
					print(err)
					return -1
				end
				rpcClient:Call("world")

				--发送普通消息
				local buff = buffer.New()
				local writer = packet.Writer(buff)
				writer:WriteI32(200)
				conn:Send(buff)			
			end)
		end
	end)

	event_loop:Run()

end

main()

