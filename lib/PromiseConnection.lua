local chuck = require("chuck")
local socket = chuck.socket
local promise = require("Promise")

local M = {}

function M.init(event_loop)
	M.event_loop = event_loop
	return M
end

local PromiseConnection = {}

PromiseConnection.__index = PromiseConnection

local function newPromiseConnection(fd)
	local c = {}
	c.conn = socket.stream.New(fd,65536)
	c = setmetatable(c, PromiseConnection)
	c.buff = chuck.buffer.New(1024)
	c.conn:Start(M.event_loop,function (data,err)
		if data then
			c.buff:AppendStr(data:Content())
			while c.promise do
				local resolve = c.promise.process(c.buff)
				if resolve then
					c.promise = promise.next
					if c.promise == nil then
						c.promiseTail = nil
					end
					resolve()					
				else
					break
				end
			end
		else
			c:Close(err)
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

function PromiseConnection:Close(delay)
	if self.conn then
		self.conn:Close(delay)
		self.conn = nil
		while self.promise do
			self.promise.reject("disconnected")
			self.promise = self.promise.next
		end
		self.promiseTail = nil
	end
end

function PromiseConnection:Send(msg)
	if self.conn == nil then
		return "invaild connection"
	elseif type(msg) ~= "string" then
		return "msg must be string"
	else
		return self.conn:Send(chuck.buffer.New(msg))
	end
end

function PromiseConnection:SetCloseCallBack(cb)
	if self.conn and cb then
		self.conn:SetCloseCallBack(cb)
	end
end

function PromiseConnection:Recv(byteCount)
	return promise.new(function(resolve,reject)
		  if nil == self.conn then
		  	reject("invaild connection")	
		  else
		  	local readPromise = {}
		  	readPromise.process = function (buff)
		  		local msg = buff:Read(byteCount)
		  		if msg then
		  			return function ()
		  				resolve(msg)
		  			end
		  		else
		  			return nil
		  		end
		  	end
		  	readPromise.reject = function(err)
		  		reject(err)
		  	end
		  	addPromise(self,readPromise)
		  end
	   end)
end

function PromiseConnection:RecvUntil(str)
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
		  			return function ()
		  				resolve(buff:Read(e))
		  			end		  			
		  		else
		  			return nil
		  		end
		  	end
		  	readPromise.reject = function(err)
		  		reject(err)
		  	end
		  	addPromise(self,readPromise)
		  end
	   end)
end

function M.connect(ip,port,timeout)
	timeout = timeout or 0
	return promise.new(function(resolve,reject)
      if nil == M.event_loop then
      	reject("use event_loop init module first")
      else
		local err = socket.stream.ip4.dail(M.event_loop,ip,port,timeout,function (fd,errCode)
			if errCode then
				reject("connect error:" .. errCode)
			else
				resolve(newPromiseConnection(fd))
			end
		end)

		if err then
			reject(err)
		end

      end
   end)
end

function M.listen(ip,port,onClient)
	return socket.stream.ip4.listen(M.event_loop,ip,port,function (fd)
		onClient(newPromiseConnection(fd))
	end)
end

return M