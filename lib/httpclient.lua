local chuck = require("chuck")

local http_response_max_header_size    = 1024*256         --http响应包头最大大小256K
local http_response_max_content_length = 2*1024*1024*1024 --http响应content最大大小2G

local http_response = {}

local eventLoop


function http_response.new(packet)
  local o = {}
  o.__index = http_response     
  setmetatable(o,o)
  o.packet = packet
  return o
end

function http_response:Method()
	return self.packet:GetMethod()
end

function http_response:URL()
	return self.packet:GetURL()
end

function http_response:Status()
	return self.packet:GetStatus()
end

function http_response:Body()
	return self.packet:GetBody()
end

function http_response:Header(filed)
	--内部吧field全转化成小写，所以也要把filed转成小写来查询
	return self.packet:GetHeader(string.lower(filed))
end

function http_response:AllHeaders()
	return self.packet:GetAllHeaders()
end


local http_request = {}

function http_request.new()
  local o = {}
  o.__index = http_request     
  setmetatable(o,o)
  o.packet = chuck.http.HttpPacket()
  if not o.packet then
  	return nil
  end
  return o
end

function http_request:AppendBody(body)
	if self.packet:AppendBody(body) then
		return "return AppendBody failed"
	else
		return "OK"
	end
end

function http_request:SetHeader(filed,value)
	--content-length不许手动设置发包时跟根据body长度生成	
	if string.lower(filed) ~= "content-length" then
		self.packet:SetHeader(filed,value)
	end
end

local connection = {}

function connection.new(host,fd)
  local o = {}
  o.__index = connection     
  setmetatable(o,o)
  o.conn = chuck.http.Connection(fd,http_response_max_header_size,http_response_max_content_length)
  if not o.conn then
  	return nil,"call http.Connection() failed"
  end
  o.host = host
  o.pendingResponse = {}
  local ret = o.conn:Start(eventLoop,function (httpPacket,err)
	if err then
		if o.pendingResponse then
			o.pendingResponse(nil,"connection lose") --通告对端关闭	
		end
	else
		if not o.pendingResponse then
			--没有请求
			return	
		end

		local response = http_response.new(httpPacket)
		local OnResponse = o.pendingResponse
		o.pendingResponse = nil
		OnResponse(response)
	end
  end)
  
  if ret then
  	return nil,"call Bind() failed"
  end
  
  return o
end

function connection:Close()
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

	request = request or http_request.new()

	path = path or "/"

	local ret = self.conn:SendRequest("1.1",self.host,method,path,request.packet)
	if not ret then
		self.pendingResponse = OnResponse
		return "OK"
	end

	return "send request failed"
	
end

function connection:Get(path,request,OnResponse)
	return SendRequest(self,"GET",path,request,OnResponse)
end

function connection:Post(path,request,OnResponse)
	return SendRequest(self,"POST",path,request,OnResponse)
end

local http_client = {}

function http_client.new(ip,port)
  local o = {}
  o.__index = http_client
  o.__gc = o.close  
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

	if not self.conn then
		local socket = chuck.socket
		socket.stream.ip4.dail(eventLoop,self.ip,self.port,function (fd,errCode)
			self.connecting = false
			if errCode then
				onResponse(nil,"connect failed") --用空response回调，通告出错
				return
			end
			self.conn = connection.new(self.ip,fd)
			if "OK" ~= SendRequest(self.conn,method,path,request,onResponse) then
				--发送失败用空response回调，通告出错
				onResponse(nil,"send request failed")
			end
		end)
		self.connecting = true		
	else
		return SendRequest(self.conn,method,path,request,onResponse)
	end
end

function http_client:Get(path,request,onResponse)
	return easySendRequest(self,"GET",path,request,onResponse)
end

function http_client:Post(path,request,onResponse)
	return easySendRequest(self,"POST",path,request,onResponse)
end

function http_client:Close()
	print("http_client:Close()")
	if self.conn then
		self.conn:Close()
		self.conn = nil
	end
end

local M = {}

function M.init(event_loop)
	if nil == eventLoop then
		eventLoop = event_loop
	end
	return M
end

function M.Request()
	return http_request.new()
end

function M.new(ip,port)
	return http_client.new(ip,port)
end

return M

