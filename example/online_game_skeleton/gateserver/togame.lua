local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local log = chuck.log
local netcmd = require("netcmd_define")
local packet = chuck.packet
local rpc = require("rpc")

local M = {
	conn = nil,
	timer = nil,
	loger = nil,
}

M.Send  = function (msg)
	if not M.conn then
		return "connection close"
	end

	if not M.ok then
		return "connection close"
	end

	local err = M.conn:Send(msg)

	if err then
		M.logger:Log(log.debug,"send to game failed:" .. err)
	end

	return ret
end

M.Start = function (server_config,service_info,event_loop,logger,onGameMsg)
	
	M.logger = logger

	onGameMsg = onGameMsg or function() end

	local gameserver = server_config.gameserver
	if not gameserver then
		return "server_config error"
	end

	local ip = gameserver.ip
	local port = gameserver.port

	if (not ip) or (not port) then
		return "server_config error"
	end

	local reConnect = nil

	reConnect = function (delay)
	   M.timer = event_loop:AddTimer(delay,function ()
			local err = socket.stream.ip4.dail(event_loop,ip,port,function (fd,errCode)
				if 0 ~= errCode then
					print("connect error:" .. errCode)
					reConnect(1000)
					M.timer = nil
					return
				end
				M.logger:Log(log.info,string.format("connect to gameserver %s:%d ok",ip,port))
				M.conn = socket.stream.New(fd,4096,packet.Decoder(65536))
				if M.conn then
					M.rpcClient = rpc.RPCClient(M.conn)
					M.conn:Start(event_loop,function (data,err)
						if data then 
							local reader = packet.Reader(data)
							local cmd = reader:ReadI16(cmd)
							if cmd == netcmd.CMD_RPC then
								rpc.OnRPCMsg(M.conn,reader:ReadStr())
							else
								onGameMsg(data)
							end
						else
							print("on disconnected",err)
							local conn = M.conn 
							M.conn:Close()
							rpc.OnConnClose(conn)
							M.rpcClient = nil 
							M.conn = nil
							reConnect(1000) 
						end
					end)

					M.rpcClient:Call("ServerLogin",function (err,result)
						if err then
							if err ~= "connection loss" then
								local conn = M.conn
								M.conn:Close()
								M.rpcClient = nil 
								M.conn = nil
								rpc.OnConnClose(conn)
							end
							M.logger:Log(log.error,string.format("login to gameserver %s:%d error:%s",ip,port,err))							
						else
							M.logger:Log(log.info,string.format("login to gameserver %s:%d ok",ip,port))	
							M.ok = true
						end
					end,service_info)

				end
			end)

	      if err then
	         M.logger:Log(log.error,string.format("connect to gameserver %s:%d error:%s",ip,port,err))
	         reConnect(1000)
	      end
	      M.timer = nil       
	      return -1
	   end)
	end	

	reConnect(0)

end


--rpc call

M.Login = function (sessionNo,account,callback)
	if not M.rpcClient then
		return "connection closed" 
	else
		return M.rpcClient:Call("PlayerLogin",callback,sessionNo,account)
	end
end

M.Logout = function (sessionNo,callback)
	if not M.rpcClient then
		return "connection closed" 
	else
		return M.rpcClient:Call("PlayerLogout",callback,sessionNo)
	end
end

return M