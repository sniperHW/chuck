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

local stream_socket = {}

function stream_socket:new(fd)
	local o = {}   
	o.__index = stream_socket
	o.__gc = function () 
			print("stream_socket gc") 
			o:Close() 
		end
	setmetatable(o, o)
	o.fd = fd	  
	return o
end

function stream_socket:Close(errno)
	if self.conn then
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

function stream_socket:Send(packet,sendFinish)
	if self.conn then
		local ret = self.conn:Send(packet,sendFinish)
		if ret >= 0 or ret == -err.EAGAIN then
			return true
		end
	end
end

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

function stream_socket:SetRecvTimeout(timeout)
	if self.conn then
		self.conn:SetRecvTimeout(timeout)
	end
end

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

local function stream_socket_connect(host,port,callback,noblock,timeout)
	local fd =  socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)
	if noblock then set_noblock(fd,1) end
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
		connector:Add2Engine(engine,
				            function(fd,errno)
				               __callback(fd,errno)
					           connector = nil 
				             end)
	else
		close(fd)
		return false
	end
	return true
end


return {
	stream = {
		listen = stream_socket_listen,
		connect = stream_socket_connect,
		packetdecoder = chuck.decoder.connection.rpacket,
		rawdecoder = chuck.decoder.connection.rawpacket,
		httpdecoder = chuck.decoder.connection.http,
	},
}