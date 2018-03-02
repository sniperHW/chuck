
local M = {}

local linkItemPool = {}
linkItemPool.head = {}
linkItemPool.head.next = nil

function linkItemPool.get()
	local item = linkItemPool.head.next
	if item then
		linkItemPool.head.next = item.next
		item.next = nil
		return item
	else
		return {}
	end
end

function linkItemPool.put(item)
	item.next = linkItemPool.head.next
	linkItemPool.head.next = item
end


local fifo = {}
fifo.__index = fifo

function M.new()
	local o = {}
	o = setmetatable(o,fifo)
	return o
end

function fifo:push(v)
	if v then
		local item = linkItemPool.get()
		item.v = v
		if nil == self.tail then
			self.head = item 
			self.tail = item
		else
			self.tail.next = item
			self.tail = item
		end
	else
		error("fifo push nil")
	end
end

function fifo:pop()
	local item = self.head
	if item then
		self.head = item.next
		if self.head == nil then
			self.tail = nil
		end
		local v = item.v
		linkItemPool.put(item)
		return v	
	else
		return nil
	end
end

function fifo:empty()
	return self.head == nil
end

function fifo:clear()
	while true do
		if nil == self:pop() then
			return
		end
	end
end


return M