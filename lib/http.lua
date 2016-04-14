local chuck = require("chuck")


local http_request_max_header_size     = 1024*256         --http请求包头最大大小256K
local http_request_max_content_length  = 4*1024*1024      --http请求content最大大小4M
local http_response_max_header_size    = 1024*256         --http响应包头最大大小256K
local http_response_max_content_length = 2*1024*1024*1024 --http响应content最大大小2G

local http_packet_readonly = {}

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
	return self.packet:AppendBody(body)
end

function http_packet_writeonly:SetHeader(filed,value)
	return self.packet:SetHeader(filed,value)
end

local http_server = {}

function http_server.new(eventLoop,fd,onRequest)
  if not eventLoop then
  	return nil,"eventLoop is nil"
  end

  if not onRequest then
  	return nil,"onRequest is nil"  	
  end	

  local o = {}
  o.__index = http_server     
  setmetatable(o,o)
  o.conn = chuck.http.Connection(fd,http_request_max_header_size,http_request_max_content_length)
  if not o.conn then
  	return nil,"call http.Connection() failed"
  end

  local ret = o.conn:Bind(eventLoop,function (httpPacket)
	if not httpPacket then
		o.conn:Close()
		o.conn = nil
		return
	end
	local request = http_packet_readonly.new(httpPacket)
	local response = http_packet_writeonly.new()
	response.End = function (self,status,phase)
		if not o.conn then
			return
		end
		status = status or "200"
		phase = phase or "OK"
		return o.conn:SendResponse("1.1",status,phase,self.packet)		
	end
	onRequest(request,response)
  end)
  
  if ret then
  	return nil,"call Bind() failed"
  end
  
  return true
end

local http_client = {}

function http_client.new(eventLoop,host,fd)
  if not eventLoop then
  	return nil,"eventLoop is nil"
  end

  local o = {}
  o.__index = http_client     
  setmetatable(o,o)
  o.conn = chuck.http.Connection(fd,http_response_max_header_size,http_response_max_content_length)
  if not o.conn then
  	return nil,"call http.Connection() failed"
  end
  o.host = host
  local ret = o.conn:Bind(eventLoop,function (httpPacket)

	if not httpPacket then
		if o.pendingResponse then
			o.pendingResponse(nil) --通告对端关闭	
		end	
		o.conn:Close()
		o.conn = nil	
		return
	end

	print("got packet")

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
	self.conn:Close()
	self.conn = nil
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

	self.pendingResponse = OnResponse

	return ret
	
end

function http_client:Get(path,request,OnResponse)
	return SendRequest(self,"GET",path,request,OnResponse)
end

function http_client:Post(path,request,OnResponse)
	return SendRequest(self,"Post",path,request,OnResponse)
end

return {
	HttpServer = function (eventLoop,fd,onRequest)
		return http_server.new(eventLoop,fd,onRequest)
	end,

	HttpClient = function (eventLoop,host,fd)
		return http_client.new(eventLoop,host,fd)
	end,

	HttpRequest = function ()
		return http_packet_writeonly.new()
	end
}

