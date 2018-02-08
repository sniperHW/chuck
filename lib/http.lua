local chuck = require("chuck")


local http_request_max_header_size     = 1024*256         --http请求包头最大大小256K
local http_request_max_content_length  = 4*1024*1024      --http请求content最大大小4M
local http_response_max_header_size    = 1024*256         --http响应包头最大大小256K
local http_response_max_content_length = 2*1024*1024*1024 --http响应content最大大小2G

local http_packet_readonly = {}

local eventLoop


function http_packet_readonly.new(packet)
  local o = {}
  o.__index = http_packet_readonly     
  setmetatable(o,o)
  o.packet = packet
  return o
end

function http_packet_readonly:Method()
	return self.packet:GetMethod()
end

function http_packet_readonly:URL()
	return self.packet:GetURL()
end

function http_packet_readonly:Status()
	return self.packet:GetStatus()
end

function http_packet_readonly:Body()
	return self.packet:GetBody()
end

function http_packet_readonly:Header(filed)
	--内部吧field全转化成小写，所以也要把filed转成小写来查询
	return self.packet:GetHeader(string.lower(filed))
end

function http_packet_readonly:AllHeaders()
	return self.packet:GetAllHeaders()
end


local http_packet_writeonly = {}

function http_packet_writeonly.new()
  local o = {}
  o.__index = http_packet_writeonly     
  setmetatable(o,o)
  o.packet = chuck.http.HttpPacket()
  if not o.packet then
  	return nil
  end
  return o
end

function http_packet_writeonly:AppendBody(body)
	if self.packet:AppendBody(body) then
		return "return AppendBody failed"
	else
		return "OK"
	end
end

function http_packet_writeonly:SetHeader(filed,value)
	--content-length不许手动设置发包时跟根据body长度生成	
	if string.lower(filed) ~= "content-length" then
		self.packet:SetHeader(filed,value)
	end
end

local http_server = {}

function http_server.new(fd,onRequest)

  if not onRequest then
  	return nil,"onRequest is nil"  	
  end	

  local conn = chuck.http.Connection(fd,http_request_max_header_size,http_request_max_content_length)
  if not conn then
  	return nil,"call http.Connection() failed"
  end

  local ret = conn:Start(eventLoop,function (httpPacket)
	if not httpPacket then
		print("http conn close")
		conn:Close()
		conn = nil
		return
	end
	local request = http_packet_readonly.new(httpPacket)
	request.conn = conn
	local response = http_packet_writeonly.new()
	response.Finish = function (self,status,phase)
		if not conn then
			return
		end
		status = status or "200"
		phase = phase or "OK"
		if conn:SendResponse("1.1",status,phase,self.packet) then
			return "send response failed"
		else
			return "OK"
		end		
	end
	onRequest(request,response)
  end)
  
  if ret then
  	return nil,"call Bind() failed"
  end
  
  return true
end

local http_client = {}

function http_client.new(host,fd)
  local o = {}
  o.__index = http_client     
  setmetatable(o,o)
  o.conn = chuck.http.Connection(fd,http_response_max_header_size,http_response_max_content_length)
  if not o.conn then
  	return nil,"call http.Connection() failed"
  end
  o.host = host
  o.pendingResponse = {}
  local ret = o.conn:Start(eventLoop,function (httpPacket)
	if not httpPacket then
		if o.pendingResponse then
			o.pendingResponse(nil,"connection lose") --通告对端关闭	
		end	
		o.conn:Close()
		o.conn = nil	
		return
	end

	if not o.pendingResponse then
		--没有请求
		return	
	end

	local response = http_packet_readonly.new(httpPacket)
	local OnResponse = o.pendingResponse
	o.pendingResponse = nil
	OnResponse(response)
  end)
  
  if ret then
  	return nil,"call Bind() failed"
  end
  
  return o
end

function http_client:Close()
	if self.conn then
		self.conn:Close()
		self.conn = nil
	end
end


local function SendRequest(self,method,path,request,OnResponse)
	if not self.conn then
		return "http_client close"
	end

	if not OnResponse then
		return "OnResponse is nil"
	end

	if self.pendingRequest then
		return "a request is watting for response"
	end

	request = request or http_packet_writeonly.new()

	path = path or "/"

	local ret = self.conn:SendRequest("1.1",self.host,method,path,request.packet)
	if not ret then
		self.pendingResponse = OnResponse
		return "OK"
	end

	return "send request failed"
	
end

function http_client:Get(path,request,OnResponse)
	return SendRequest(self,"GET",path,request,OnResponse)
end

function http_client:Post(path,request,OnResponse)
	return SendRequest(self,"POST",path,request,OnResponse)
end

local easy_http_server = {}

function easy_http_server.new(onRequest)

  if not onRequest then
  	return nil,"onRequest is nil"  	
  end	

  local o = {}
  o.__index = easy_http_server   
  setmetatable(o,o)
  o.onRequest = onRequest
  return o
end

function easy_http_server:Listen(ip,port)
	if self.server then
		return self,"server already running" 
	end
	local socket = chuck.socket
	local onRequest = self.onRequest
	self.server = socket.stream.ip4.listen(eventLoop,ip,port,function (fd,err)
		if err then
			return
		end
		http_server.new(fd,onRequest)
	end)

	print(ip,port,self.server)

	if not self.server then
		return self,"Listen on " .. ip .. ":" .. port .. " failed" 
	end

	return self,"OK" 
end

function easy_http_server:Close()
	if self.server then
		self.server:Close()
		self.server = nil
	end
end


local easy_http_client = {}

function easy_http_client.new(ip,port)
  local o = {}
  o.__index = easy_http_client
  o.__gc = function ()
  	if o.client then
  		o.client:Close()
  	end
  end     
  setmetatable(o,o)
  o.ip = ip
  o.port = port
  return o
end

local function easySendRequest(self,method,path,request,onResponse)
	
	if self.connecting then
		return "connecting"
	end
	
	if not onResponse then
		return "onResponse is nil"
	end

	if not self.client then
		local socket = chuck.socket
		socket.stream.ip4.dail(eventLoop,self.ip,self.port,function (fd,errCode)
			self.connecting = false
			if errCode then
				onResponse(nil,"connect failed") --用空response回调，通告出错
				return
			end
			self.client = http_client.new(self.ip,fd)
			if "OK" ~= SendRequest(self.client,method,path,request,onResponse) then
				--发送失败用空response回调，通告出错
				onResponse(nil,"send request failed")
			end
		end)
		self.connecting = true		
	else
		return SendRequest(self.client,method,path,request,onResponse)
	end
end

function easy_http_client:Get(path,request,onResponse)
	return easySendRequest(self,"GET",path,request,onResponse)
end

function easy_http_client:Post(path,request,onResponse)
	return easySendRequest(self,"POST",path,request,onResponse)
end

function easy_http_client:Close()
	if self.client then
		self.client:Close()
		self.client = nil
	end
end


local M = {

	Request = function ()
		return http_packet_writeonly.new()
	end,

	Server = function (onRequest)
		return easy_http_server.new(onRequest)
	end,

	Client = function (ip,port)
		return easy_http_client.new(ip,port)
	end
}

function M.init(event_loop)
	if nil == eventLoop then
		eventLoop = event_loop
	end
	return M
end

local lower = string.lower

function M.Upgrade(request,response)

	local method = request:Method()
	if nil == method or lower(method) ~= "get" then
		request.conn:Close()
		return nil,"invaild upgrade request"		
	end
	local connection = request:Header("connection")
	if nil == connection or lower(connection) ~= "upgrade" then
		request.conn:Close()
		return nil,"invaild upgrade request"
	end
	local upgrade = request:Header("upgrade")
	if  nil == upgrade or upgrade ~= lower("websocket") then
		request.conn:Close()
		return nil,"invaild upgrade request"
	end
	local Sec_Websocket_Version = request:Header("Sec-Websocket-Version")
	if nil == Sec_Websocket_Version or Sec_Websocket_Version ~= "13" then
		request.conn:Close()
		return nil,"invaild upgrade request"
	end

	local Sec_WebSocket_Key = request:Header("Sec-WebSocket-Key")
	if nil == Sec_WebSocket_Key then
		request.conn:Close()
		return nil,"invaild upgrade request"
	end

	local Sec_WebSocket_Accept = chuck.base64.encode(chuck.crypt.sha1(Sec_WebSocket_Key .. "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
	response:SetHeader("Upgrade","websocket")
	response:SetHeader("Connection","Upgrade")
	response:SetHeader("Sec-WebSocket-Accept",Sec_WebSocket_Accept)
	local ret = response:Finish("101","OK")
	if "OK" == ret then
		local wsconn = upgradeOk(chuck.websocket.Upgrade(request.conn))
		if not wsconn then
			request.conn:Close()
			return nil,"upgrade failed"
		else
			return wsconn
		end
	else
		request.conn:Close()
		return nil,ret
	end
end

return M

