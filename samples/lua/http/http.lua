local chuck = require("chuck")
local socket_helper = chuck.socket_helper
local decoder = chuck.decoder
local packet = chuck.packet

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
	local response_packet = packet.rawpacket(self:buildResponse())
	if self.KeepAlive then
		self.connection:Send(response_packet)
	else
		self.connection:Send(response_packet,function ()
			self.connection:Close()
		end)
	end
end


local http_request = {}

function http_request:new(path)
  local o = {}
  o.__index = http_request      
  setmetatable(o,o)
  o.path = path
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

function http_server:new()
  local o = {}
  o.__index = http_server      
  setmetatable(o,o)
  return o
end

function http_server:CreateServer(engine,ip,port,on_request)
	local fd =  socket_helper.socket(socket_helper.AF_INET,
								 socket_helper.SOCK_STREAM,
								 socket_helper.IPPROTO_TCP)
	if not fd then
		return nil
	end
	socket_helper.addr_reuse(fd,1)
	if 0 == socket_helper.listen(fd,ip,port) then
		local acceptor = chuck.acceptor(fd)
		acceptor:Add2Engine(engine,function (client)
			local conn = chuck.connection(client,4096,decoder.http(65535))
			conn:Add2Engine(engine,function (_,rpk,err)
				if not conn then
					return
				end
				if rpk then
					--for k,v in pairs(rpk:Headers()) do
					--	print(k,v)
					--end
					local response = http_response:new()
					response.connection = conn
					response.KeepAlive  = rpk:KeepAlive()
					if not on_request(rpk,response) then
						if not response.KeepAlive then
							conn = nil
						end
						return 
					end
				end
				conn:Close()
				conn = nil
			end)			
		end)
		self.acceptor = acceptor
		return self
	end
	return nil	
end

local function HttpServer(engine,ip,port,on_request)
	return http_server:new():CreateServer(engine,ip,port,on_request)
end


local httpclient = {}

function httpclient:new(host,port,KeepAlive)
  local o     = {}
  o.__index   = httpclient      
  setmetatable(o,o)
  o.host      = host
  o.port      = port or 80
  o.KeepAlive = KeepAlive
  return o
end

function httpclient:buildRequest(request)
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


function httpclient:request(engine,method,request,on_response)

	local function SendRequest()
		request.method = method
		if self.KeepAlive then
			request:WriteHead({"Connection: Keep-Alive"})
		end
		self.conn:Send(packet.rawpacket(self:buildRequest(request)))
	end

	if not self.conn then
		local fd = socket_helper.socket(socket_helper.AF_INET,
										socket_helper.SOCK_STREAM,
										socket_helper.IPPROTO_TCP)

		if not fd then
			return false
		end

		local function connect_callback(fd,err)
			if err ~= 0 then
				on_response(nil)
				socket_helper.close(fd)
			else
				self.conn = chuck.connection(fd,4096,decoder.http(65535))
				if self.conn then
					self.conn:Add2Engine(engine,function (_,res,err)
						on_response(res)
						if not self.KeepAlive then
							self.conn:Close()
							self.conn = nil
						end
					end)
					SendRequest()
				else
					on_response(nil)
					socket_helper.close(fd)
				end					
			end
		end

		socket_helper.noblock(fd,1)
		local ret = socket_helper.connect(fd,self.host,self.port)
		if ret == 0 then
			connect_callback(fd,0)
		elseif ret == -err.EINPROGRESS then
			local connector = chuck.connector(fd,5000)
			connector:Add2Engine(engine,function(fd,errnum)
				connect_callback(fd,errnum)
				connector = nil 
			end)
		else
			return false
		end
	else
		SendRequest()
	end	
	return true
end

function httpclient:Post(engine,request,on_response)
	return self:request(engine,"POST",request,on_response)
end

function httpclient:Get(engine,request,on_response)
	return self:request(engine,"GET",request,on_response)
end

local function HttpClient(host,port)
	return httpclient:new(host,port)
end

local function HttpRequest(path)
	return http_request:new(path)
end 


return {
	HttpServer  = HttpServer,
	HttpClient  = HttpClient,
	HttpRequest = HttpRequest,
}
