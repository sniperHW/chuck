local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local log = chuck.log
local netcmd = require("netcmd_define")
local packet = chuck.packet
local rpc = require("rpc")

local M = {
	rooms = {}
}

local room = {}
room.__index = room

local function newRoom(conn,rpcClient,index)
	local r = {}
	r.conn = conn
	r.index = index
	r.rpcClient = rpcClient
	r = setmetatable(r, room)
	return r
end

function room:Send(msg)
	if not self.conn then
		return "connection closed"
	else
		local err = self.conn:Send(msg)
		if err then
			M.logger:Log(log.info,"send to room:" .. self.index .. " failed:" .. err)
		end
		return err
	end
end



M.GetRoom = function (index)
	return M.rooms[index]	
end


M.Start = function (server_config,service_info,event_loop,logger,onRoomMsg)
	M.logger = logger
	M.event_loop = event_loop

	onRoomMsg = onRoomMsg or function() end

	if not server_config.roomserver then
		return "server_config error"
	end

	local reConnect = nil

	reConnect = function (ip,port,delay)
	   local timer
	   timer = M.event_loop:AddTimer(delay,function ()
			local err = socket.stream.ip4.dail(M.event_loop,ip,port,function (fd,errCode)
				if errCode then
					print("connect error:" .. errCode,ip,port)
					reConnect(ip,port,1000)
					timer = nil
					return
				end
				M.logger:Log(log.info,string.format("connect to roomserver %s:%d ok",ip,port))
				local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
				if conn then					
					conn:Start(event_loop,function (data)
						if data then 
							local reader = packet.Reader(data)
							local cmd = reader:ReadI16(cmd)
							if cmd == netcmd.CMD_RPC then
								rpc.OnRPCMsg(conn,reader:ReadStr())
							else
								onGameMsg(data)
							end
						else 
							conn:Close()
							room.conn = nil
							reConnect(ip,port,1000) 
						end
					end)

					local rpcClient = rpc.RPCClient(conn)
					rpcClient:Call("ServerLogin",function (err,result)
						if err then
							if err == "connection loss" then
								conn:Close()
								rpc.OnConnClose(conn)
							end 							
							logger:Log(log.error,string.format("login to roomserver %s:%d error:%s",ip,port,err))							
						else
							local index = ip .. ":" .. port
							local room = M.rooms[index]
							if not room then
								room = newRoom(conn,rpcClient,index)
								M.rooms[index] = room
							else
								room.conn = conn
								room.rpcClient = rpcClient
							end	
							logger:Log(log.info,string.format("login to roomserver %s:%d ok",ip,port))													
						end
					end,service_info)
				end
			end)

	      if err then
	         M.logger:Log(log.error,string.format("connect to roomserver %s:%d error:%s",ip,port,err))
	         reConnect(ip,port,1000)
	      end
	 	  timer = nil	       
	      return -1
	   end)
	end	

	for k,v in pairs(server_config.roomserver) do
		reConnect(v.ip,v.port,0)
	end

end

return M