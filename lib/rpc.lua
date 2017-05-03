-- a sample rpc
local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local packet = chuck.packet
local log = chuck.log

local cmd_ping = 1
local cmd_request = 2
local cmd_response = 3

local M = {}
M.recvbuff = 4096
M.max_packet_size = 65535
M.seq = 1
M.keepalive = 10 --10秒内没收到心跳响应认为连接断开
M.heartbeat = M.keepalive/2

local logger = log.CreateLogfile("RPC")

local method = {}

function M.registerMethod(methodName,func)
	method[methodName] = func
end

local rpcResponse = {}
rpcResponse.__index = rpcResponse

local function newResponse(conn,seqno)
	local r = {}
	r.conn = conn
	r.seqno = seqno
	r = setmetatable(r, rpcResponse)
	return r
end

local function sendResponse(response,result,err)
	if response.seqno > 0 then
		local resp = {err = err,ret=result}
		local buff = buffer.New(64)
		local writer = packet.Writer(buff)
		writer:WriteI8(cmd_response)
		writer:WriteI64(response.seqno)
		writer:WriteTable(resp)
		response.conn:Send(buff)
	end
end

function rpcResponse:Return(...)
	local result = {...}
	if #result == 0 then
		result = nil
	end
	sendResponse(self,result)
end

local function callMethod(methodName,args,response)
	local func = method[methodName]
	if nil == func then
		logger:Log(log.error,string.format("callMethod method not found:%s",methodName))
		sendResponse(response,nil,"method not found:" .. methodName)
	else
		local errmsg
		local success,ret = xpcall(func,function (err)
		    	errmsg = err
			end,response,table.unpack(args))
		if nil ~= errmsg then
			logger:Log(log.error,string.format("error on callMethod:%s",errmsg))
			sendResponse(response,nil,errmsg)
		end
	end
end

local function onRequest(conn,msg)
	local rpacket = packet.Reader(msg)
	local cmd   = rpacket:ReadI8()

	if cmd == cmd_ping then
		local buff = buffer.New(512)
		local writer = packet.Writer(buff)
		writer:WriteI8(cmd_ping)
		conn:SendUrgent(buff)
	elseif cmd == cmd_request then
		local seqno = rpacket:ReadI64()
		local name  = rpacket:ReadStr()
		local args  = rpacket:ReadTable()
		local response = newResponse(conn,seqno)
		callMethod(name,args,response)
	else
		logger:Log(log.error,string.format("onRequest unkonw cmd:%d",cmd))
	end
end


function M.startServer(eventLoop,ip,port)
	local server = socket.stream.ip4.listen(eventLoop,ip,port,function (fd)
		local conn = socket.stream.New(fd,M.recvbuff,packet.Decoder(M.max_packet_size))
		if conn then
			conn:Start(eventLoop,function (data)
				if data then
					onRequest(conn,data)
				else
					conn:Close() 
				end
			end)
		end
	end)
	return server
end

local rpcClient = {}
rpcClient.__index = rpcClient

local function newClient(conn,onDisconnected)
	local c = {}
	c.conn = conn
	c.onDisconnected = onDisconnected
	c.callbacks = {}
	c = setmetatable(c, rpcClient)	
	return c
end

function rpcClient:onPacket(msg)
	local rpacket = packet.Reader(msg)
	local cmd     = rpacket:ReadI8()
	if cmd == cmd_response then
		local seqno = rpacket:ReadI64()
		local cb = self.callbacks[seqno]
		if cb then
			self.callbacks[seqno]= nil
			local resp = rpacket:ReadTable()
			xpcall(cb,function (err)
			    logger:Log(log.error,string.format("error on rpc callback:%s",err))
			end,resp.err,resp.ret)
		end
	elseif cmd == cmd_ping then
		self.lastHeartBeat = os.time() * 1000
	else 
		logger:Log(log.error,string.format("rpcClient:onPacket unkonw cmd:%d",cmd))
	end
end

function rpcClient:onDisconnected()

	for k,cb in pairs(self.callbacks) do
		xpcall(cb,function (err)
			logger:Log(log.error,string.format("error on rpc callback:%s",err))
		end,"connection loss",nil)
	end

	if self.disconnectCB then
		self.disconnectCB()
	end
end

function rpcClient:Close()
	if self.conn then
		self.conn:Close()
		self.conn = nil
		if self.timer then
			self.timer:UnRegister()
			self.timer = nil
		end
		self:onDisconnected()
	end
end

function rpcClient:startHeartBeat(eventLoop)
	self.lastHeartBeat = os.time() * 1000
	self.timer = eventLoop:AddTimer(1000*M.heartbeat,function ()
		local now = os.time() * 1000
		if now - self.lastHeartBeat >= M.keepalive * 1000 then
			self.timer = nil
			self:Close()
			return -1	
		else
			local buff = buffer.New(64)
			local writer = packet.Writer(buff)
			writer:WriteI8(cmd_ping)
			self.conn:SendUrgent(buff)
		end		
	end)
end

function rpcClient:Call(methodName,callback,...)

	if nil == methodName then
		return "methodName == nil"
	end

	if nil == self.conn then
		return "connection loss"
	end

	local args = {...}
	local seqno = M.seq

	local buff = buffer.New(512)
	local writer = packet.Writer(buff)
	writer:WriteI8(cmd_request)
	if nil == callback then
		--不关心返回值，seqno设为0
		writer:WriteI64(0)
	else
		writer:WriteI64(seqno)
		M.seq = M.seq + 1
	end
	writer:WriteStr(methodName)
	writer:WriteTable(args)
	
	if nil ~= self.conn:Send(buff) then
		return "send request failed"
	else
		if callback then
			self.callbacks[seqno] = callback
		end
	end	
end


function M.connectServer(eventLoop,ip,port,connectCB,onDisconnected)
	return socket.stream.ip4.dail(eventLoop,ip,port,function (fd,errCode)
		if 0 ~= errCode then
			connectCB(nil,errCode)
			return
		end
		local conn = socket.stream.New(fd,M.recvbuff,packet.Decoder(M.max_packet_size))
		if conn then
			local client = newClient(conn,onDisconnected)
			client:startHeartBeat(eventLoop)
			conn:Start(eventLoop,function (data)
				if data then 
					client:onPacket(data)
				else 
					client:Close()
				end
			end)
			connectCB(client,nil)
		end
	end)
end

return M