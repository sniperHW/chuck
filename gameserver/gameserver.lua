--暂不使用数据库
package.cpath = './lib/?.so;'
package.path = './gameserver/?.lua;'

local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local event_loop = chuck.event_loop.New()
local log = chuck.log
local config = require("config")
local user_session = require("user_session")
local netcmd = require("netcmd_define")
--local db = require("db")
local room = require("room")
local gameuser = require("user")
local packet = chuck.packet

local netcmd_handlers = {}
local users = {} --userid -> user

GameLog = log.CreateLogfile("GameServer");

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


--local function init_db()
--	return db.init()
--end

--local function load_from_db(session,userid)
	--从数据库载入用户数据，如果是新用户创建

--end

register_netcmd_handler(netcmd.CMD_CS_LOGIN,function(session,msg)
	if session.user then
		session:Close()
		return
	end
	local userid = msg:ReadStr()
	local user = users[userid]
	if user then
		local old_session = user.session
		if old_session then
			--断开老连接
			old_session:Close()
		end
		user.session = session
	else
		user = gameuser.New(session,userid)
		users[userid] = user
	end	

	--通告登录成功
	local buff = chuck.buffer.New()
	local w = packet.Writer(buff)
	w:WriteI16(netcmd.CMD_SC_LOGIN)
	w:WriteStr(userid)
	session:Send(buff)

end)

register_netcmd_handler(netcmd.CMD_CS_ENTER_ROOM,function(session,msg)
	local user = session.user
	if not user then
		return
	end

	if user.room then
		return
	end

	room.EnterRoom(user)

	local buff = chuck.buffer.New()
	local w = packet.Writer(buff)
	w:WriteI16(netcmd.CMD_SC_ENTER_ROOM)
	session:Send(buff)	

end)

register_netcmd_handler(netcmd.CMD_CS_MOVE,function(session,msg)
	local user = session.user
	if not user then
		return
	end

	if not user.room then
		return
	end

	user.room:Move(user,msg)	

end)

register_netcmd_handler(netcmd.CMD_CS_KILL,function(session,msg)
	local user = session.user
	if not user then
		return
	end

	if not user.room then
		return
	end

	user.room:Kill(user,msg)	

end)

register_netcmd_handler(netcmd.CMD_CS_RELIVE,function(session,msg)
	local user = session.user
	if not user then
		return
	end

	if not user.room then
		return
	end

	user.room:Relive(user)	

end)

register_netcmd_handler(netcmd.CMD_CS_LEAVE_ROOM,function(session,msg)
	local user = session.user
	if not user then
		return
	end

	if not user.room then
		return
	end

	user.room:Leave(user)	

end)

register_netcmd_handler(netcmd.CMD_CS_CLIENT_ENTER_ROOM,function(session,msg)
	local user = session.user
	if not user then
		return
	end

	if not user.room then
		return
	end

	user.room:ClientEnter(user)	

end)

local function OnConnLose(session)
	local user = session.user 
	if user then
		if user.room then
			user.room:Leave(user)
		end
		users[user.userid] = nil
	end
end

local function start_server()
	--if not init_db() then
	--	GameLog:Log(log.error,"call init_db() failed")
	--	return
	--end

	--启动服务器监听
	local server = socket.stream.ip4.listen(event_loop,config.server_ip,config.server_port,function (fd)
		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
			local session = user_session.New(conn)
			conn:Start(event_loop,function (data)
				if data then
					--接收到数据包，执行处理
					local rpacket = packet.Reader(data)
					local cmd = rpacket:ReadI16()
					on_netcmd(session,cmd,rpacket)
				else
					OnConnLose(session)
				end
			end)			
		end
	end)

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		GameLog:Log(log.info,"recv SIGINT stop server")
		event_loop:Stop()
	end)

	GameLog:Log(log.info,string.format("GameServer start on %s:%d",config.server_ip,config.server_port))

	event_loop:Run()
end


start_server()

GameLog:Log(log.info,"GameServer stop")
