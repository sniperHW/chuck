-- a sample rpc protocal

local chuck = require("chuck")
local buffer = chuck.buffer
local packet = chuck.packet
local promise
local log = chuck.log

local cmd_request = 1
local cmd_response = 2

local M = {}
M.timeout = 10 --调用超时时间
M.seq = 1
M.clients = {}

--以下7个函数用户可以重定义
function M.pack(buff)
	return buffer.New(buff)
end

function M.serializeArg(writer,...)
	writer:WriteTable({...})
end

function M.unserializeArg(reader)
	return reader:ReadTable()
end

function M.serializeMethod(writer,method)
	writer:WriteStr(method)
end

function M.unserializeMethod(reader)
	return reader:ReadStr()
end

function M.serializeResp(writer,err,...)
	local result = {...}
	if #result == 0 then
		result = nil
	end
	writer:WriteTable({err=err,ret=result})
end

function M.unserializeResp(reader)
	local resp = reader:ReadTable()
	return resp.err,resp.ret
end

--

function M.init(event_loop)
	if nil == M.event_loop then
		M.event_loop = event_loop
		promise = require("Promise").init(event_loop)
	end
	return M
end

local logger = log.CreateLogfile("RPC")

local methods = {}

function M.registerMethod(method,func)
	methods[method] = func
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

local function sendResponse(response,err,...)
	if response.seqno > 0 then
		local buff = buffer.New()
		local writer = packet.Writer(buff)
		writer:WriteI8(cmd_response)
		writer:WriteI64(response.seqno)
		M.serializeResp(writer,err,...)
		buff = M.pack(buff:Content())
		local err = response.conn:Send(buff)
		if err then
			logger:Log(log.error,string.format("error on sendResponse:%s",err))
		end
	end
end

function rpcResponse:Return(...)
	sendResponse(self,nil,...)
end

function rpcResponse:Error(errmsg)
	sendResponse(self,errmsg)
end

local function callMethod(methodName,args,response)
	local func = methods[methodName]
	if nil == func then
		logger:Log(log.error,string.format("callMethod method not found:%s",methodName))
		response:Error("method not found:" .. methodName)
	else
		local errmsg
		local success,ret = xpcall(func,function (err)
		    	errmsg = err
			end,response,table.unpack(args))
		if nil ~= errmsg then
			logger:Log(log.error,string.format("error on callMethod:%s",errmsg))
			response:Error(errmsg)
		end
	end
end

function M.OnRPCMsg(conn,msg)
	local buff = buffer.New(msg)
	local rpacket = packet.Reader(buff)
	local cmd   = rpacket:ReadI8()
	if cmd == cmd_request then
		local seqno = rpacket:ReadI64()
		local method = M.unserializeMethod(rpacket)
		local args  = M.unserializeArg(rpacket)
		local response = newResponse(conn,seqno)
		callMethod(method,args,response)
	elseif cmd == cmd_response then
		local rpcclient = M.clients[conn]
		if not rpcclient then
			return
		end
		local seqno = rpacket:ReadI64()
		local cb_info = rpcclient.callbacks[seqno]
		if cb_info then
			rpcclient.callbacks[seqno]= nil
			cb_info.timer:UnRegister()
			local err,ret = M.unserializeResp(rpacket)
			xpcall(cb_info.cb,function (err)
			    logger:Log(log.error,string.format("error on rpc callback:%s",err))
			end,err,ret)
		end
	else
		logger:Log(log.error,string.format("onRequest unkonw cmd:%d",cmd))
	end
end

local rpcClient = {}
rpcClient.__index = rpcClient

--在一个conn上只能建立一个rpcclient,如果重复建立会失败返回nil
function M.RPCClient(conn)	
	if M.clients[conn] then
		return nil
	end
	local c = {}
	c.conn = conn
	c.callbacks = {}
	c = setmetatable(c, rpcClient)
	M.clients[conn] = c	
	return c
end

function rpcClient:Call(method,callback,...)

	if nil == method then
		return "method == nil"
	end

	if nil == self.conn then
		return "connection loss"
	end

	local seqno = callback and M.seq or 0
	M.seq = M.seq + 1

	local buff = buffer.New()
	local writer = packet.Writer(buff)
	writer:WriteI8(cmd_request)
	writer:WriteI64(seqno)
	M.serializeMethod(writer,method)
	M.serializeArg(writer,...)
	buff = M.pack(buff:Content())
	
	if nil ~= self.conn:Send(buff) then
		return "send request failed"
	else
		if callback then
			local callback_info = {}
			callback_info.cb = callback
			callback_info.timer = M.event_loop:AddTimer(M.timeout * 1000,function ()
				callback_info.timer = nil
				self.callbacks[seqno] = nil
				xpcall(callback,function (err)
					logger:Log(log.error,string.format("error on rpc callback:%s",err))
				end,"timeout",nil)
				return -1
			end)
			self.callbacks[seqno] = callback_info
		end
	end	
end

function rpcClient:CallPromise(methodName,...)
	local args = {...}
	return promise.new(function (resolve,reject)
		local err = self:Call(methodName,function (err,result)
				if err then
					reject(err)
				else
					resolve(result)
				end
			end,table.unpack(args))
		if err then
			reject(err)
		end
	end)
end

function M.OnConnClose(conn)
	local client = M.clients[conn]
	if client then
		M.clients[conn] = nil
		for _,cb_info in pairs(client.callbacks) do
			cb_info.timer:UnRegister()
			xpcall(cb_info.cb,function (err)
				logger:Log(log.error,string.format("error on rpc callback:%s",err))
			end,"connection loss",nil)
		end		
	end
end

return M