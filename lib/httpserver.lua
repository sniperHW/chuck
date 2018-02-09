local chuck = require("chuck")

local http_request_max_header_size     = 1024*256         --http请求包头最大大小256K
local http_request_max_content_length  = 4*1024*1024      --http请求content最大大小4M

local http_request = {}

local eventLoop


function http_request.new(packet)
  local o = {}
  o.__index = http_request     
  setmetatable(o,o)
  o.packet = packet
  return o
end

function http_request:Method()
	return self.packet:GetMethod()
end

function http_request:URL()
	return self.packet:GetURL()
end

function http_request:Status()
	return self.packet:GetStatus()
end

function http_request:Body()
	return self.packet:GetBody()
end

function http_request:Header(filed)
	--内部吧field全转化成小写，所以也要把filed转成小写来查询
	return self.packet:GetHeader(string.lower(filed))
end

function http_request:AllHeaders()
	return self.packet:GetAllHeaders()
end


local http_response = {}

function http_response.new()
  local o = {}
  o.__index = http_response     
  setmetatable(o,o)
  o.packet = chuck.http.HttpPacket()
  if not o.packet then
  	return nil
  end
  return o
end

function http_response:AppendBody(body)
	if self.packet:AppendBody(body) then
		--记录日志
	end
	return self
end

function http_response:SetHeader(filed,value)
	--content-length不许手动设置发包时跟根据body长度生成	
	if string.lower(filed) ~= "content-length" then
		self.packet:SetHeader(filed,value)
	end
	return self
end

function on_client(fd,onRequest)
  local conn = chuck.http.Connection(fd,http_request_max_header_size,http_request_max_content_length)
  if not conn then
  	return nil,"call http.Connection() failed"
  end

  local ret = conn:Start(eventLoop,function (httpPacket,err)
	if httpPacket then
		local request = http_request.new(httpPacket)
		request.conn = conn
		local response = http_response.new()
		response.Finish = function (self,status,phase)
			status = status or "200"
			phase = phase or "OK"
			if conn:SendResponse("1.1",status,phase,self.packet) then
				return "send response failed"
			else
				return "OK"
			end		
		end
		onRequest(request,response)
	else
		print("client disconnect:" .. err)
	end
  end)
  
  if ret then
  	return nil,"call Bind() failed"
  end
  
  return true
end

local http_server = {}

function http_server.new(onRequest)
  local o = {}
  o.__index = http_server   
  setmetatable(o,o)
  if nil == onRequest then
  	onRequest = function (request,response)
  		response:SetHeader("Content-Type","text/plain")
		response:Finish("200","OK")
  	end
  else  
  	o.onRequest = onRequest
  end
  return o
end

function http_server:Listen(ip,port)
	if self.server then
		return self,"server already running" 
	end
	local socket = chuck.socket
	local onRequest = self.onRequest
	self.server = socket.stream.ip4.listen(eventLoop,ip,port,function (fd,err)
		if err then
			return
		end
		on_client(fd,onRequest)
	end)

	if not self.server then
		return self,"Listen on " .. ip .. ":" .. port .. " failed" 
	end

	return self,"OK" 
end

function http_server:Close()
	if self.server then
		self.server:Close()
		self.server = nil
	end
end

local M = {}

function M.init(event_loop)
	if nil == eventLoop then
		eventLoop = event_loop
	end
	return M
end

function M.new(onRequest)
	return http_server.new(onRequest)
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

