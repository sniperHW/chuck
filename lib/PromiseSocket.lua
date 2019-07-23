local chuck = require("chuck")
local socket = chuck.socket
local buffer = chuck.buffer
local promise

local M = {}

function M.init(event_loop)
	M.event_loop = event_loop
	promise = require("Promise").init(event_loop)
	return M
end

local PromiseSocket = {}

PromiseSocket.__index = PromiseSocket

function PromiseSocket.new(fd)
	local c = {}
	c.conn = socket.stream.socket(fd,65536)
	c = setmetatable(c, PromiseSocket)
	c.buff = buffer.New(1024)

	c.conn:SetCloseCallBack(function ()
		while c.promise do
			local p = c.promise
			if p.timer then
				p.timer:UnRegister()
			end
			p.reject("disconnected")
			c.promise = p.next
		end
		c.promiseTail = nil

		if c.onClose then
			c.onClose()
		end
	end)


	c.conn:Start(M.event_loop,function (data,err)
		if data then
			c.buff:AppendStr(data:Content())
			while c.promise do
				local p = c.promise
				if p.process(c.buff) then
					c.promise = p.next
					if c.promise == nil then
						c.promiseTail = nil
					end					
				else
					break
				end
			end
			if c.buff:Size() > 512*1024 then
				--接收太快,暫時停止接收
				c.conn:PauseRead()
				c.needResume = true
			end
		else
			c:Close(0)
		end
	end)
	return c
end

local function addPromise(conn,promise)
	if conn.promise == nil then
		conn.promise = promise
	else
		conn.promiseTail.next = promise
	end
	conn.promiseTail = promise
end

function PromiseSocket:Close(delay)
	if self.conn then
		self.conn:Close(delay)
		self.conn = nil
	end
end

function PromiseSocket:Send(msg)
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

function PromiseSocket:OnClose(cb)
	if self.conn and cb then
		self.onClose = cb
	end
end

local function onRecvTimeout(c,readPromise)
	local pre = nil
	local cur = c.promise
	while cur do
		if cur == readPromise then
			if pre == nil then
				--首元素
				c.promise = cur.next
				if c.promise == nil then
					c.promiseTail = nil
				end
			else
				pre.next = cur.next
				if c.promiseTail == cur then
					--尾元素
					c.promiseTail = pre
				end
			end
			cur.next = nil
			cur.reject("timeout")
			break
		else
			pre = cur
			cur = cur.next
		end
	end
end

function PromiseSocket:Recv(byteCount,timeout)
	return promise.new(function(resolve,reject)
		  if nil == self.conn then
		  	reject("invaild connection")	
		  else
		  	local readPromise = {}
		  	readPromise.process = function (buff)
		  		local msg = buff:Read(byteCount)
		  		if msg then
		  			if self.needResume then
						self.needResume = false
						self.conn:ResumeRead()
					end	
		  			resolve(msg)
		  			return true
		  		else
		  			return false
		  		end
		  	end

		  	if self.buff and readPromise.process(self.buff) then
		  		return
		  	else
		  		readPromise.reject = function(err)
		  			reject(err)
		  		end
		  		addPromise(self,readPromise)

				if timeout then
					readPromise.timer = M.event_loop:AddTimerOnce(timeout,function ()
						readPromise.timer = nil
						onRecvTimeout(self,readPromise)
					end)
				end	
		  	end
		  end
	   end)
end

function PromiseSocket:RecvUntil(str,timeout)
	return promise.new(function(resolve,reject)
		  if nil == self.conn then
		  	reject("invaild connection")
		  elseif nil == str or "" == str then
		  	reject("str is empty")	
		  else
		  	local readPromise = {}
		  	readPromise.process = function (buff)
		  		local s,e = string.find(buff:Content(), str)
		  		if s then
					if self.needResume then
						self.needResume = false
						self.conn:ResumeRead()
					end			  			
		  			resolve(buff:Read(e))
		  			return true	  			
		  		else
		  			return false
		  		end
		  	end
		  	if self.buff and readPromise.process(self.buff) then
		  		return
		  	else
		  		readPromise.reject = function(err)
		  			reject(err)
		  		end
		  		addPromise(self,readPromise)
				if timeout then
					readPromise.timer = M.event_loop:AddTimerOnce(timeout,function ()
						readPromise.timer = nil
						onRecvTimeout(self,readPromise)
					end)
				end		  		
		  	end
		  end
	   end)
end

function M.connect(ip,port,timeout)
	timeout = timeout or 0
	return promise.new(function(resolve,reject)
      if nil == M.event_loop then
      	reject("use event_loop init module first")
      else
      	local addr = socket.addr(socket.AF_INET,ip,port)
		local err = socket.stream.dial(M.event_loop,addr,function (fd,errCode)
			if errCode then
				reject("connect error:" .. errCode)
			else
				resolve(PromiseSocket.new(fd))
			end
		end)

		if err then
			reject(err)
		end

      end
   end)
end

function M.listen(ip,port,onClient)
    local addr = socket.addr(socket.AF_INET,ip,port)	
	return socket.stream.listen(M.event_loop,addr,function (fd)
		onClient(PromiseSocket.new(fd))
	end)
end

return M