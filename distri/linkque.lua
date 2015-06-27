local LinkQue = {}

function LinkQue:new()
	  local o = {}   
	  o.__index = LinkQue
	  setmetatable(o, o)	  
	  o.size = 0
	  o.head = {}
	  o.tail = {}
	  o.size = 0
	  o.head.__pre = nil
	  o.head.__next = o.tail
	  o.tail.__next = nil
	  o.tail.__pre = o.head
	  return o
end

function LinkQue:Push(node)
	if node.__owner then
		return 
	end
	self.tail.__pre.__next = node
	node.__pre = self.tail.__pre
	self.tail.__pre = node
	node.__next = self.tail
	node.__owner = self
	self.size = self.size + 1	

end

function LinkQue:Front()
    if self.size > 0 then
	return self.head.__next
    else
	return nil
    end	
end

function LinkQue:Remove(node)
	if node.__owner == self and self.size > 0 then
		node.__pre.__next = node.__next
		node.__next.__pre = node.__pre
		node.__next = nil
		node.__pre = nil
		node.__owner = nil
		self.size = self.size - 1
		return true
	end
	return false
end

function LinkQue:Pop()
	if self.size > 0 then
		local node = self.head.__next
		self:Remove(node)
		return node
	else
		return nil
	end
end

function LinkQue:IsEmpty()
	return self.size == 0
end

function LinkQue:Len()
	return self.size
end

return {
	New = function () return LinkQue:new() end
}