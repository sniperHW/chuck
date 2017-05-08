dofile("./example/online_game_skeleton/common.lua")
local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local log = chuck.log
local netcmd = require("netcmd_define")
local db = require("db")
local packet = chuck.packet
local json   = require("cjson")
local errorcode = require("errorcode")
local rpc = require("rpc")
--local roomPlayer = require("roomplayer")

local serverIP
local serverPort 

local event_loop = chuck.event_loop.New()
local logger = log.CreateLogfile("roomserver")

local connectionMgr = {}

local function onGateCmd(conn,msg)

end

rpc.registerMethod("ServerLogin",function (response,service_info)
	connectionMgr[response.conn] = service_info
	logger:Log(log.info,string.format("%s %s login ok",service_info.type,service_info.index))
	response:Return("ok")
end)

local function onInitOk()
	--启动服务器监听
	local server = socket.stream.ip4.listen(event_loop,serverIP,serverPort,function (fd,err)
		if err then
			logger:Log(log.info,"accept error:" .. err)
			return
		end		
		local conn = socket.stream.New(fd,4096,packet.Decoder(65535))
		if conn then
			conn:Start(event_loop,function (msg,err)
				local service_info = connectionMgr[conn]
				if err then
					conn:Close()
					connectionMgr[conn] = nil
					if service_info and service_info.type == "gate" then
						--通告玩家模块，gate连接断开
						--roomPlayer:OnGateDisconnected(conn)
					end
				else
					local reader = packet.Reader(msg)
					local cmd = reader:ReadI16(cmd)
					if cmd == netcmd.CMD_RPC then
						rpc.OnRPCMsg(conn,reader:ReadStr())
					else
						if service_info and service_info.type == "gate" then
							onGateCmd(conn,msg)
						end
					end
				end
			end)			
		end
	end)

	logger:Log(log.info,string.format("roomserver start on %s:%d",serverIP,serverPort))

end

local function init()
	return db.init(event_loop,onInitOk)
end

if arg == nil or #arg ~= 2 then
	print("useage:lua roomserver ip port")
else
	serverIP = arg[1]
	serverPort = arg[2]
	init()
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		logger:Log(log.info,"recv SIGINT stop server")
		event_loop:Stop()
	end)
	event_loop:Run()
	logger:Log(log.info,string.format("roomserver stop %s:%d",serverIP,serverPort))
end