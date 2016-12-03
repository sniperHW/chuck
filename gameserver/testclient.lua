package.cpath = './lib/?.so;'
package.path = './gameserver/?.lua;'

local chuck = require("chuck")
local socket = chuck.socket
local packet = chuck.packet
local config = require("config")
local netcmd = require("netcmd_define")

local event_loop = chuck.event_loop.New()
local netcmd_handlers = {}

local function register_netcmd_handler(cmd,handler)
	netcmd_handlers[cmd] = handler
end

local function on_netcmd(session,cmd,msg)
	local handler = netcmd_handlers[cmd]
	if not handler then
		GameLog:Log(log.error,"unknow cmd:" .. cmd)
	else
		local ret,err = pcall(handler,session,msg)
		if not ret then
			GameLog:Log(log.error,string.format("call %d handler failed:%s",cmd,err))
		end
	end
end


register_netcmd_handler(netcmd.CMD_SC_LOGIN,function(session,msg)
	print("CMD_SC_LOGIN")
	--通告登录成功,请求进入房间
	local buff = chuck.buffer.New()
	local w = packet.Writer(buff)
	w:WriteI16(netcmd.CMD_CS_ENTER_ROOM)
	session:Send(buff)
end)

register_netcmd_handler(netcmd.CMD_SC_ENTER_ROOM,function(session,msg)
	print("CMD_SC_ENTER_ROOM")
	--服务器完成进入,客户端完成进入通告服务器
	local buff = chuck.buffer.New()
	local w = packet.Writer(buff)
	w:WriteI16(netcmd.CMD_CS_CLIENT_ENTER_ROOM)
	session:Send(buff)
end)

register_netcmd_handler(netcmd.CMD_SC_BEGIN_SEE,function(session,msg)
	print("CMD_SC_BEGIN_SEE")
	--服务器完成进入,客户端完成进入通告服务器
	local userID = msg:ReadStr()
	print("see " .. userID)
end)

if not arg then

	print("useage lua testclient.lua userID")

else
	socket.stream.ip4.dail(event_loop,config.server_ip,config.server_port,function (fd,errCode)
		if 0 ~= errCode then
			print("connect error:" .. errCode)
			return
		end
		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
			conn:Start(event_loop,function (data)
				if data then 
					local rpacket = packet.Reader(data)
					local cmd = rpacket:ReadI16()
					on_netcmd(conn,cmd,rpacket)
				else
					print("client disconnected") 
					conn:Close()
				end
			end)
		end

		--发送login
		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteI16(netcmd.CMD_CS_LOGIN)
		w:WriteStr(arg[1])--userID
		conn:Send(buff)	

	end)

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		print("recv SIGINT stop client")
		event_loop:Stop()
	end)

	event_loop:Run()

end