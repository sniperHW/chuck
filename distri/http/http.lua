local chuck         = require("chuck")
local engine        = require("distri.engine")
local socket        = require("distri.socket")
local httppacket    = chuck.packet.rawpacket
local httpdecoder   = socket.stream.decoder.http
local err           = chuck.error
local socket_helper = chuck.socket_helper

local http = {
	recvbuff  = 4096,        --套接口接收缓冲大小
	maxpacket = 1024*128,    --最大http包大小
}

local http_response = {}

function http_response:new()
  local o = {}
  o.__index = http_response      
  setmetatable(o,o)
  return o
end

function http_response:buildResponse()
	local strResponse  = string.format("HTTP/1.1 %d %s\r\n",self.status,self.phase)
	for k,v in pairs(self.headers) do
		strResponse = strResponse .. string.format("%s\r\n",v)
	end	
	if self.body then
		strResponse = strResponse .. string.format("Content-Length: %d \r\n\r\n%s",#self.body,self.body)
	else
		strResponse = strResponse .. "\r\n\r\n"
	end
	return strResponse
end

function http_response:WriteHead(status,phase,heads)
	self.status = status
	self.phase = phase
	self.headers = self.headers or {}
	if heads then
		for k,v in pairs(heads) do
			table.insert(self.headers,v)
		end
	end
end

function http_response:End(body)
	self.body = body
	local response_packet = httppacket(self:buildResponse())
	self.connection:Send(response_packet)
end


local http_request = {}

function http_request:new(path)
  local o = {}
  o.__index = http_request      
  setmetatable(o,o)
  o.path = path
  o.headers = {}
  return o
end

function http_request:WriteHead(heads)
	self.headers = self.headers or {}
	if heads then
		for k,v in pairs(heads) do
			table.insert(self.headers,v)
		end
	end
end

function http_request:End(body)
	self.body = body
end


local http_server = {}

function http_server:new(on_request)
  local o      = {}
  o.__index    = http_server
  o.on_request = on_request      
  setmetatable(o,o)
  return o
end


function http_server:Listen(ip,port)
	self.server = socket.stream.Listen(ip,port,function (s,errno)
		if s then
			s:Ok(http.recvbuff,httpdecoder(http.maxpacket),function (_,msg,errno)
				if not s then
					return
				end
				if msg then
					local response = http_response:new()
					response.connection = s
					if not self.on_request(msg,response) then
						return 
					end
				elseif s then
					s:Close(errno)
					s = nil
				end
			end)
		end
	end)
	if self.server then
		return self
	else
		return nil
	end
end


local host2ip = {}

local function getHostIp(host)
	if host2ip[host] then
		return host2ip[host][1]
	else
		local ips = socket_helper.gethostbyname_ipv4(host)
		if ips then
			host2ip[host] = ips 
			return ips[1]
		end
	end
end

local http_client = {}

function http_client:new(host,port)
  local ip = getHostIp(host)
  if not ip then return end
  local o     = {}
  o.__index   = http_client      
  setmetatable(o,o)
  o.host      = host
  o.ip        = ip
  o.port      = port or 80
  o.requests  = {}  
  return o
end

function http_client:Close()
	if self.conn then
		self.conn:Close()
		self.conn = nil		
		for k,v in pairs(self.requests) do
			v[2](nil,0)
		end
		self.requests = {}		
	end
end

function http_client:buildRequest(request)
	local strRequest = string.format("%s %s HTTP/1.1\r\n",request.method,request.path)
	strRequest = strRequest .. string.format("Host: %s \r\n",self.host)
	for k,v in pairs(request.headers) do
		strRequest = strRequest .. string.format("%s\r\n",v)
	end	
	if request.body then
		strRequest = strRequest .. string.format("Content-Length: %d \r\n\r\n%s",#request.body,request.body)
	else
		strRequest = strRequest .. "\r\n"
	end
	return strRequest
end


function http_client:request(method,request,on_response)
  request.method = method
  table.insert(self.requests,{request,on_response})

  local function SendRequest()
  		if #self.requests > 0 then
			local request = self.requests[1][1]
			self.conn:Send(httppacket(self:buildRequest(request)))
		end
  end

  local function OnResponse(res,errno)
  		if #self.requests > 0 then
			local on_response = self.requests[1][2]
			table.remove(self.requests,1)
			on_response(res,errno)
		end  	
  end

  if #self.requests == 1 then
	    if not self.conn then
			local function connect_callback(s,errno)
				if errno ~= 0 then
					for k,v in pairs(self.requests) do
						v[2](nil,errno)
					end
					self.requests  = {}
				else
					s:Ok(http.recvbuff,httpdecoder(http.maxpacket),function (_,res,errno)
							if not self.conn then
								return
							end
							OnResponse(res,errno)
							if errno or not self.KeepAlive then							
								self.conn:Close(errno)
								self.conn = nil
								for k,v in pairs(self.requests) do
									v[2](nil,errno)
								end
								self.requests = {}									
							end
							--send another request if any
							SendRequest()
						end)
					self.conn = s
					--connected,send the first request
					SendRequest()			
				end
			end
	    	local errno = socket.stream.Connect(self.ip,self.port,connect_callback)
	    	if errno then
	    		return false
	    	end
		else
			--no other pending request,send immediate
			SendRequest()
		end
	end	
	return true
end

function http_client:Post(request,on_response)
	return self:request("POST",request,on_response)
end

function http_client:Get(request,on_response)
	return self:request("GET",request,on_response)
end

http.Server = function (...)
	return http_server:new(...)
end

http.Client = function (...)
	return http_client:new(...)
end

http.Request = function (...)
	return http_request:new(...)
end

return http
