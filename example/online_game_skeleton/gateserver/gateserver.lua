dofile("./example/online_game_skeleton/common.lua")
local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local log = chuck.log
local netcmd = require("netcmd_define")
local db = require("db")
local packet = chuck.packet
local togame = require("togame")
local toroom = require("toroom")
local json   = require("cjson")
local errorcode = require("errorcode")
--local rpc    = require("rpc")

local user_session_counter = 1
local serverIP
local serverPort 

user_sessions = {}
local event_loop = chuck.event_loop.New()
local logger = log.CreateLogfile("gateserver")

local function sendToClient(session,msg)
	if not session.conn then
		return "session closed"
	else
		return session.conn:Send(msg)
	end
end

local function closeSession(session,delay)
	delay = delay or 0
	if session.conn then
		session.conn:Close(delay)
		session.conn = nil
	end
	user_sessions[session.sessionNo] = nil
end

local function onConnLose(session)

	if session.gameID then
		--通告gameserver连接断开
		local msg = buffer.New()
		local writer = packet.Writer(msg)
		writer:WriteI64(session.sessionNo)
		writer:WriteI16(netcmd.CMD_GG_CLIENT_DISCONNECTED)
		togame:Send(msg)
	end

	--通告roomserver连接断开
	if session.room then
		local msg = buffer.New()
		local writer = packet.Writer(msg)
		writer:WriteI64(session.sessionNo)
		writer:WriteI16(netcmd.CMD_GR_CLIENT_DISCONNECTED)
		session.room:Send(msg)
	end
	
	closeSession(session)
end

--处理来自roomserver的消息
local function onRoomServerMsg(msg)

end

--处理来自gameserver的消息
local function onGameServerMsg(msg)
	local rpacket = packet.Reader(msg)
	local cmd = rpacket:ReadI16(cmd)

	if cmd == netcmd.CMD_GG_KICK then
		local target = rpacket:ReadI64()
		local session = user_sessions[target]
		if session then
			msg = buffer.New()
			local writer = packet.Writer(msg)
			writer:WriteI16(netcmd.CMD_SC_KICK)
			--session.conn:Send(msg)
			sendToClient(session,msg)
			closeSession(session,1000)
		end
	else
		local targets = rpacket:ReadTable()
		msg = buffer.New(rpacket:ReadStr())
		for k,v in pairs(targets) do 
			local session = user_sessions[v]
			if session then
				sendToClient(session,msg)
				--session.conn:Send()
			end
		end
	end
end

--处理来自客户端的消息
local function onClientMsg(session,msg)
	local rpacket = packet.Reader(msg)
	local cmd = rpacket:ReadI16()
	if cmd == netcmd.CMD_CS_LOGIN then
		local account = rpacket:ReadStr()
		local err = togame.Login(session.sessionNo,account,function (err,result)
			msg = buffer.New()
			local writer = packet.Writer(msg)
			writer:WriteI16(netcmd.CMD_SC_LOGIN)
			if err then
				logger:log(log.error,"togame.Login() error:" .. err)
				writer:WriteI16(errorcode.err_system_busy)
			else
				session.gameID = result[1]
				writer:WriteI16(errorcode.err_ok)
			end
			sendToClient(session,msg)
			--session.conn:Send(msg)
		end)

		if err then
			msg = buffer.New()
			local writer = packet.Writer(msg)
			writer:WriteI16(netcmd.CMD_SC_LOGIN)
			writer:WriteI16(errorcode.err_system_busy)
			sendToClient(session,msg)
			--session.conn:Send(msg)
			logger:log(log.error,"togame.Login() failed:" .. err)
		end

	elseif cmd == netcmd.CMD_CS_LOGOUT then

		local err = togame.Logout(session.sessionNo,function (err)
			msg = buffer.New()
			local writer = packet.Writer(msg)
			writer:WriteI16(netcmd.CMD_SC_LOGOUT)
			if err then
				logger:log(log.error,"togame.Login() error:" .. err)
				writer:WriteI16(errorcode.err_system_busy)
				sendToClient(session,msg)
				--session.conn:Send(msg)
			else
				writer:WriteI16(errorcode.err_ok)
				sendToClient(session,msg)	
				--session.conn:Send(msg)
				closeSession(session,1000)
			end
		end)

		if err then
			msg = buffer.New()
			local writer = packet.Writer(msg)
			writer:WriteI16(netcmd.CMD_SC_LOGOUT)
			writer:WriteI16(errorcode.err_system_busy)
			sendToClient(session,msg)		
			--session.conn:Send(msg)
			logger:log(log.error,"togame.Logout() failed:" .. err)
		end

	else
		if cmd > netcmd.CMD_CS_BEGIN and cmd < netcmd.CMD_CS_END then
			local forword_msg = buffer.New(msg:size() + 12) --8:sessionNo + 4:bin len
			local writer = packet.Writer(forword_msg)
			writer:WriteI64(session.sessionNo)
			writer:WriteStr(msg:Content())
			togame:Send(forword_msg)
		elseif cmd > netcmd.CMD_CR_BEGIN and cmd < netcmd.CMD_CR_END and session.room then
			local forword_msg = buffer.New(msg:size() + 12) --8:sessionNo + 4:bin len
			local writer = packet.Writer(forword_msg)
			writer:WriteI64(session.sessionNo)
			writer:WriteStr(msg:Content())
			session.room:Send(forword_msg)		
		else
			logger:log(log.debug,"onClientMsg() invaild cmd:" .. cmd)
		end
	end
end




local function onInitOk()
	--启动服务器监听
	local server = socket.stream.ip4.listen(event_loop,serverIP,serverPort,function (fd)
		local conn = socket.stream.New(fd,4096,packet.Decoder(65535))
		if conn then
			local session = {conn=conn,sessionNo=user_session_counter}
			user_session_counter = user_session_counter + 1

			local err = conn:Start(event_loop,function (msg,err)
				if err then
					onConnLose(session,err)
				else
					onClientMsg(session,msg)
				end
			end)
			if err then
				logger:Log(log.error,"conn:Start() error:" .. err)
				conn:Close()				
			end
			user_sessions[session.sessionNo] = session
		end
	end)

	logger:Log(log.info,string.format("gateserver start on %s:%d",serverIP,serverPort))

end

local function init()
	
	local on_db_init_ok = function ()
		db.execute(function (reply,err)
			if err then
				logger:Log(log.error,"get server_config error:" .. err)
				event_loop:Stop()
			else
				local server_config = json.decode(reply)
				local service_info = {type="gate",index= serverIP .. ":" .. serverPort}
				local err1 = togame.Start(server_config,service_info,event_loop,logger,onGameServerMsg)
				--local err2 = toroom.Start(server_config,service_info,event_loop,logger,onRoomServerMsg)

				if err1 or err2 then
					logger:Log(log.error,"start togame or toroom failed")
					event_loop:Stop()
				else
					onInitOk()
				end

			end
		end,"get","server_config")
	end

	return db.init(event_loop,on_db_init_ok)
end

if arg == nil or #arg ~= 2 then
	print("useage:lua gateserver ip port")
else
	serverIP = arg[1]
	serverPort = arg[2]
	init()

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		logger:Log(log.info,"recv SIGINT stop server")
		event_loop:Stop()
	end)

	event_loop:Run()

	logger:Log(log.info,string.format("gateserver stop %s:%d",serverIP,serverPort))

end