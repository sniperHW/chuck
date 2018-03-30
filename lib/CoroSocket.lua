local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local coroutine = require("ccoroutine")

local event_loop

local M = {}

function M.init(eventLoop)
	event_loop = eventLoop
	return M
end

local CoroSocket = {}
CoroSocket.__index = CoroSocket

function CoroSocket.new(fd)
	local c = setmetatable({},CoroSocket)
	c.conn = socket.stream.socket(fd,65536)
	c.buff = buffer.New(1024)
	c.waitting = {}
	c.conn:SetCloseCallBack(function ()
		local waitting = c.waitting
		c.waitting = {}
		local size = #waitting
		for i = 1,size do
			local context = waitting[i]
			if context.timer then
				context.timer:UnRegister()
				context.timer = nil
			end
			coroutine.resume(context.co,nil,"socket close")
		end
		if c.onClose then
			c.onClose()
		end
	end)
	c.conn:Start(event_loop,function (data,err)
		if data then
			c.buff:AppendStr(data:Content())
			local waitting = c.waitting
			c.waitting = {}
			local size = #waitting
			for i = 1,size do
				local context = waitting[i]
				local msg = context:Read(c.buff)
				if msg then
					--喚醒等待的coroutine
					if context.timer then
						context.timer:UnRegister()
						context.timer = nil
					end
					coroutine.resume(context.co,msg)
				else
					--重添加到waitting
					table.insert(c.waitting,context)
				end
			end
			if c.buff:Size() > 512*1024 then
				--接收太快,暫時停止接收
				c.conn:PauseRead()
				c.needResume = true
			end
		else
			c:Close(err)
		end
	end)
	return c	
end


local readContext = {}
readContext.__index = readContext

function readContext.new(co,readFunc)
	local o = setmetatable({},readContext)
	o.co = co
	o.readFunc  = readFunc 
	return o
end

function readContext:Read(buff)
	return self.readFunc(buff)
end

function CoroSocket:Recv(byteCount,timeout)
	local co = coroutine.running()
	if co == nil then
		return nil,"Recv must call under coroutine context"
	elseif byteCount <= 0 then
		return nil,"byteCount:" .. byteCount
	else
		local msg = self.buff:Read(byteCount)
		if msg then
			if self.needResume then
				self.needResume = false
				self.conn:ResumeRead()
			end			
			return msg
		elseif self.conn == nil then
			return nil,"socket close"
		else
			local context = readContext.new(co,function ()
				return self.buff:Read(byteCount)
			end)
			table.insert(self.waitting,context)
			if timeout then
				context.timer = event_loop:AddTimerOnce(timeout,function ()
					context.timer = nil
					for k,v in pairs(self.waitting) do
						if v == context then
							table.remove(self.waitting,k)
							break
						end
					end
					coroutine.resume(co,nil,"timeout")
				end)
			end			
			return coroutine.yield()
		end
	end
end

function CoroSocket:RecvUntil(str,timeout)
	if nil == str then
		return nil,"str == nil"
	end
	local co = coroutine.running()
	if co == nil then
		return nil,"Recv must call under coroutine context"
	else

		local function readUntil(buff)
			local s,e = string.find(buff:Content(), str)
			if s then
				return buff:Read(e)
			else
				return nil
			end			
		end

		local msg = readUntil(self.buff)
		if msg then
			if self.needResume then
				self.needResume = false
				self.conn:ResumeRead()
			end
			return msg
		elseif self.conn == nil then
			return nil,"socket close"
		else
			local context = readContext.new(co,function ()
				return readUntil(self.buff)
			end)
			table.insert(self.waitting,context)
			if timeout then
				context.timer = event_loop:AddTimerOnce(timeout,function ()
					context.timer = nil
					for k,v in pairs(self.waitting) do
						if v == context then
							table.remove(self.waitting,k)
							break
						end
					end
					coroutine.resume(co,nil,"timeout")
				end)
			end
			return coroutine.yield()
		end
	end
end

function CoroSocket:Send(msg)
	if self.conn == nil then
		return "invaild connection"
	elseif type(msg) == "string" then
		return self.conn:Send(buffer.New(msg))
	else
		local mt = getmetatable(msg)
		if mt and mt.__name == "lua_bytebuffer" then
			return self.conn:Send(msg)
		else
			return "msg must be [string | lua_bytebuffer]"
		end
	end
end

function CoroSocket:OnClose(cb)
	if self.conn and cb then
		self.onClose = cb
	end
end

function CoroSocket:Close(delay)
	if self.conn then
		self.conn:Close(delay)
		self.conn = nil
	end
end

function M.ListenIP4(ip,port,onConnection)
	if event_loop == nil then
		return nil,"not init"
	else
		local addr = socket.addr(socket.AF_INET,ip,port)
		return socket.stream.listen(event_loop,addr,function (fd,err)
			local c
			if fd then
				c = CoroSocket.new(fd)
			end
			coroutine.run(function ()
				onConnection(c,err)
			end)
		end)
	end
end

function M.ConnectIP4(ip,port,timeout)
	if event_loop == nil then
		return nil,"not init" 
	else
		local co = coroutine.running()
		if co == nil then
			return nil,"ConnectIP4 must call under coroutine context"
		end
		local addr = socket.addr(socket.AF_INET,ip,port)
		local ret = socket.stream.dial(event_loop,addr,function (fd,errCode)
			if fd then
				coroutine.resume(co,CoroSocket.new(fd))
			else
				coroutine.resume(co,nil,errCode)
			end
		end,timeout)
		if ret == nil then
			return coroutine.yield()
		else
			return nil,"connect failed"
		end
	end
end

return M


