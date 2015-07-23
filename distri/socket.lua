local chuck = require("chuck")
local engine = require("distri.engine")
local socket_helper = chuck.socket_helper
local err = chuck.error
local listen = socket_helper.listen
local connect = socket_helper.connect
local AF_INET = socket_helper.AF_INET
local SOCK_STREAM = socket_helper.SOCK_STREAM
local IPPROTO_TCP = socket_helper.IPPROTO_TCP
local socket = socket_helper.socket
local addr_reuse = socket_helper.addr_reuse
local set_noblock = socket_helper.noblock
local close = socket_helper.close
local Sche  = require("distri.uthread.sche")

local stream_socket = {}

function stream_socket:new(fd)
	local o = {}   
	o.__index = stream_socket
	o.__gc = function () 
			--print("stream_socket gc") 
			o:Close() 
		end
	setmetatable(o, o)
	o.fd = fd
	o.pending_call = {}
	o.rpc_record   = {0,0}	  
	return o
end

function stream_socket:Close(errno)
	if self.conn then
		if self.pending_call then
			--连接断开,如果有未决的rpc调用,通过失败
			for k,co in pairs(self.pending_call) do				
				Sche.WakeUp(co,"socket close")
			end
		end
		self.conn:Close()
		if self.on_close then
			self.on_close(errno)
		end
	elseif self.fd then
		close(self.fd)
	end
	self.conn = nil
	self.fd = nil
	self.on_close = nil
end


--[[
	发送逻辑数据包
	packet:逻辑包,目前支持rawpacket,wpacket
	sendFinish:packet发送完成后回调
]]

function stream_socket:Send(packet,sendFinish)
	if self.conn then
		local ret = self.conn:Send(packet,sendFinish)
		if ret >= 0 or ret == -err.EAGAIN then
			return true
		end
	end
end

--[[完成数据连接建立,只有对socket对象调用Ok成功后才能进行正常的数据收发
	recvbuff_size:底层接收缓冲大小
	decoder:逻辑解包器,目前支持rawpacket,rpacket和httppacket
	on_packet(msg,errno):数据包回调,如果成功接收数据包msg!=nil,如果出错msg=nil,errno~=nil
	on_close:连接断开后回调     
]]

function stream_socket:Ok(recvbuff_size,decoder,on_packet,on_close)
	local function __on_packet(_,packet,err)
		on_packet(self,packet,err)
	end
	self.conn = chuck.connection(self.fd,recvbuff_size,decoder)
	if self.conn:Add2Engine(engine,__on_packet) then
		self.on_close = on_close
		return true
	end
end

--设置接收超时,超时on_packet返回nil包和errno=ERVTIMEOUT
function stream_socket:SetRecvTimeout(timeout)
	if self.conn then
		self.conn:SetRecvTimeout(timeout)
	end
end

--设置发送超时,超时on_packet返回nil包和errno=ESNTIMEOUT
function stream_socket:SetSendTimeout(timeout)
	if self.conn then
		self.conn:SetSendTimeout(timeout)
	end	
end

local function stream_socket_listen(host,port,callback)
	local fd =  socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)
	addr_reuse(fd,1)
	if 0 == listen(fd,host,port) then
		local acceptor = chuck.acceptor(fd)
		local function __callback(fd,errno)
			local s
			if fd then s = stream_socket:new(fd) end
				callback(s,errno)
			end
		if acceptor:Add2Engine(engine,__callback) then
			return acceptor
		end
	end
end

local stream_socket_connect

local function stream_socket_connect_sync(host,port,timeout)
	local co = Sche.Running()
	if not co then
		return "stream_socket_connect_sync should call under coroutine"
	end
	local ss
	local waiting
	local function callback(s,errno)
		ss = s
		if waiting then
			Sche.WakeUp(co)
		end
	end

	local errno = stream_socket_connect(host,port,callback,timeout)
	if errno then return errno end
	if not ss then
		waiting = true
		Sche.Wait()
	end
	return nil,ss
end

--[[
	分为阻塞connect和非阻塞connect
	要使用阻塞模式将callback参数设置成nil即可.阻塞connect只能在coroutine上下文下调用,只会阻塞当前coroutine.
	callback(s,errno):成功时s为socket对象,如果失败s=nil,errno为错误码
]]

stream_socket_connect = function (host,port,callback,timeout)
	if not callback then
		return stream_socket_connect_sync(host,port,timeout)
	end
	local fd =  socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)
	set_noblock(fd,1)
	local ret = connect(fd,host,port)
	local function __callback(fd,errno)
		local s
		if errno ~= 0 then 
			close(fd) 
		else
			s = stream_socket:new(fd)
		end
		callback(s,errno)
	end 
	if ret == 0 then
		__callback(fd,0)
	elseif ret == -err.EINPROGRESS then
		local connector = chuck.connector(fd,timeout)
		connector:Add2Engine(engine,function(fd,errno)
			__callback(fd,errno)
			connector = nil 
		end)
	else
		close(fd)
		return ret
	end
end


return {
	stream = {
		New         = function (fd) return stream_socket:new(fd) end,
		Listen      = stream_socket_listen,
		Connect     = stream_socket_connect,
		decoder = {
			rpacket = chuck.decoder.connection.rpacket,
			rawpacket = chuck.decoder.connection.rawpacket,
			http = chuck.decoder.connection.http,
		}
	},
}