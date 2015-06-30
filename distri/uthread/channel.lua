local LinkQue = require "distri.linkque"
local Shce = require "distri.uthread.sche"

local channel = {}

function channel:new()
  local o = {}
  o.__index = channel      
  setmetatable(o,o)
  o.chan = LinkQue.New()
  o.block = LinkQue.New()
  return o
end

function channel:Send(msg)
	self.chan:Push({msg})
	local co = self.block:Pop()  
	if co then
		Shce.WakeUp(co)
	end		
end

function channel:Recv()
	while true do
		local msg = self.chan:Pop()
		if not msg then
			local co = Shce.Running()
			self.block:Push(co)
			Shce.Block()
		else
			return table.unpack(msg)
		end
	end	
end

return {
	New = function () return channel:new() end
}