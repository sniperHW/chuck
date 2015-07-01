local chuck   = require("chuck")
local engine  = require("distri.engine")
local Sche    = require("distri.uthread.sche")
local LinkQue = require("distri.linkque")

local map = {}

local client = {}

function client:new(c)
  local o = {}
  o.__index = client
  o.__gc    = function() print("redis_client gc") end      
  setmetatable(o,o)
  o.c = c
  o.pending = LinkQue:New()
  return o
end

function client:Close(err)
	local co
	while true do
		co = self.pending:Pop()
		if co then
			co = co[1]
			Sche.WakeUp(co,false,err)
		else
			map[self.c] = nil
			self.c:Close()
			self.c = nil
			return
		end
	end
end

function client:Do(cmd)
	local co = Sche.Running()
	
	if not co then return "should call in a coroutine context " end
	if not self.c then return "client close" end

	if "ok" == self.c:Execute(cmd,function (_,reply)
		Sche.WakeUp(co,true,reply)
	end) then
		local node = {co}
		self.pending:Push(node)
		--[[
			如果succ == true,则reply是操作结果,
			否则,reply是传递给Close的err值
		]]--
		local succ,reply = Sche.Wait()
		self.pending:Remove(node)
		if succ then
			return nil,reply
		else
			return reply
		end	
	end
	return "error"
end

local redis = {}

function redis.Connect(ip,port,on_error)
	local err,c = chuck.redis.Connect(engine,ip,port,function (_,err)
		local c = map[_]
		if c then
			on_error(c,err)
		end
	end)
	if c then
		return err,client:new(c)
	else
		return err
	end
end

return redis


