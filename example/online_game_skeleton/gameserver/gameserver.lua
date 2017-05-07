dofile("./example/online_game_skeleton/common.lua")
local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local log = chuck.log
local netcmd = require("netcmd_define")
local db = require("db")
local packet = chuck.packet
local toroom = require("toroom")
local json   = require("cjson")
local errorcode = require("errorcode")
local rpc = require("rpc")
local playerMgr = require("gameplayer")

local serverIP
local serverPort 

local event_loop = chuck.event_loop.New()
local logger = log.CreateLogfile("gameserver")

rpc.registerMethod("ServerLogin",function (response,service_info)
	--connectionMgr[response.conn] = service_info
	logger:Log(log.info,string.format("%s %s login ok",service_info.type,service_info.index))
	response:Return("ok")
end)

local function onGateCmd(conn,msg)
	--来自Gate的普通消息直接交给玩家模块处理
    local rpacket = packet.Reader(msg)
	local gateindex = rpacket:ReadI64()
	playerMgr.OnClientCmd(conn,gateindex,rpacket:ReadStr())
end

local function onInitOk()
	--启动服务器监听
	local server = socket.stream.ip4.listen(event_loop,serverIP,serverPort,function (fd)
		local conn = socket.stream.New(fd,4096,packet.Decoder(65535))
		if conn then
			conn:Start(event_loop,function (msg,err)
				print(msg,err)
				if err then
					conn:Close()
					playerMgr.OnGateDisconnected(conn)
				else
					local reader = packet.Reader(msg)
					local cmd = reader:ReadI16(cmd)
					if cmd == netcmd.CMD_RPC then
						rpc.OnRPCMsg(conn,reader:ReadStr())
					else
						onGateCmd(conn,msg)
					end
				end
			end)			
		end
	end)

	logger:Log(log.info,string.format("gameserver start on %s:%d",serverIP,serverPort))

end

local function init()
	
	local on_db_init_ok = function ()
		db.Command("get","server_config"):Execute(function (reply,err)
			if err then
				logger:Log(log.error,"get server_config error:" .. err)
				event_loop:Stop()
			else
				local server_config = json.decode(reply)
				local service_info = {type="game",index= serverIP .. ":" .. serverPort}
				local err = toroom.Start(server_config,service_info,event_loop,logger,onRoomServerMsg)

				if err then
					logger:Log(log.error,"start toroom failed")
					event_loop:Stop()
				else
					onInitOk()
				end

			end
		end)
	end

	return db.init(event_loop,on_db_init_ok)
end

if arg == nil or #arg ~= 2 then
	print("useage:lua gameserver ip port")
else
	serverIP = arg[1]
	serverPort = arg[2]
	init()
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		logger:Log(log.info,"recv SIGINT stop server")
		event_loop:Stop()
	end)
	event_loop:Run()
	logger:Log(log.info,string.format("gameserver stop %s:%d",serverIP,serverPort))
end