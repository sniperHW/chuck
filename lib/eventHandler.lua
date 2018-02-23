
--[[
	队列式的消息处理器，提供队列处理模式和排它处理模式

	队列处理模式:RegisterHandler(mutexMode)
	只有当前队列尾巴的处理器的mutexMode == false时才能调用成功
	处于这个模式下，当事件发生时，会从队列首部开始遍历所有的处理器并回调处理函数

	例如有两个窗口需要显示同一个数据源的数据变化，则可以在这两个窗口对象中通过队列模式注册数据变化的事件处理函数。
	当数据发生变化时，按入列顺序调用回调函数处理处理窗口的刷新

	排他模式:RegisterHandler(queueMode = true)
	处理器被添加到队列尾部，处于此模式下，当事件发生时只执行尾部处理器的回调
	使用排他模式可以实现栈式的执行回调

	例如：

	有两个处理层级1,2
	开始时由层级1负责处理A事件，当某个事件发生时，创建一个新的层级2,此时希望由层级2处理A事件,则可以在层级2的初始化中
	使用排他模式注册A的处理函数。则当A发生时只由层级2注册的处理函数来处理事件。当层级2销毁时，将处理器反注册。在这之后
	A事件又重新被层级1注册的处理函数处理

]]

local M = {}

local eventHandler = {}
eventHandler.__index = eventHandler

function eventHandler.new(slot,idx,handler,mutexMode)
	local o = {}
	o = setmetatable(o,eventHandler)
	o.mutexMode = mutexMode
	o.handler = handler
	o.slot = slot
	o.idx = idx
	return o	
end

function eventHandler:OnEvent(...)
	--if "unregister" == self.handler(...) then
	--	self:UnRegister()
	--end
	return self.handler(...)
end

function eventHandler:UnRegister()
	if not self.slot then
		return "self.slot == nil"
	end

	if not self.idx then
		return "self.idx == nil"
	end

	if self ~= self.slot.handlers[self.idx] then
		return "self ~= self.slot[self.idx]"
	end
	table.remove(self.slot.handlers,self.idx)
	if self.idx ~= #self.slot.handlers + 1 then
		--修正剩余handlers的idx
		for k,v in ipairs(self.slot.handlers) do
			v.idx = k
		end
	end
	self.slot = nil
end

local handlerSlot = {}
handlerSlot.__index = handlerSlot


function handlerSlot.new()
	local o = {}
	o = setmetatable(o,handlerSlot)
	o.handlers = {}
	return o
end

function handlerSlot:OnEvent(...)
	local size = #self.handlers
	if size == 0 then
		return
	end

	local top = self.handlers[#self.handlers] 
	if top.mutexMode then
		--互斥模式，队尾的handler来处理事件
		if "unregister" == top:OnEvent(...) then
			top:UnRegister()
		end
	else
		--从队列首部开始依次处理事件

		local k = next(self.handlers)
		while k do
			local h = self.handlers[k]
			if "unregister" == h:OnEvent(...) then
				h:UnRegister()
				if k >= #self.handlers then
					return
				end
			else
				k = next(self.handlers,k)
			end
		end
	end
end

function handlerSlot:RegisterHandler(handler,mutexMode)
	local top = self.handlers[#self.handlers]
	if top and top.mutexMode and not mutexMode then
		--目前排他处理模式，不允许添加非排他模式的handler
		print("top handler in mutexMode,unable to register queueMode handler")
		return nil
	end
	local idx = #self.handlers + 1
	local evHandler = eventHandler.new(self,idx,handler,mutexMode)
	table.insert(self.handlers,evHandler)
	return evHandler
end

function handlerSlot:Clear()
	for _,v in ipairs(self.handlers) do
		v.slot = nil
	end
	self.handlers = {}	
end

function handlerSlot:ReplaceHandler(handler,mutexMode)
	--清空当前所有handler
	self:Clear()
	local evHandler = eventHandler.new(self,1,handler,mutexMode)
	table.insert(self.handlers,evHandler)
	return evHandler
end

local eventModule = {}
eventModule.__index = eventModule

function M.new()
	local e = {}
	e.slots = {}
	e = setmetatable(e,eventModule)
	return e
end

function eventModule.handler(obj, method)
    return function(...)
       return method(obj, ...)
    end
end

function eventModule:RegisterHandler(event,mutexMode,handler)

	mutexMode = mutexMode == "mutexMode"

	local slot = self.slots[event]
	if not slot then
		slot = handlerSlot.new()
		self.slots[event] = slot
	end

	return slot:RegisterHandler(handler,mutexMode)
end

--清空当前所有handler,将新的handler添加到队列中
function eventModule:ReplaceHandler(event,mutexMode,handler)

	mutexMode = mutexMode == "mutexMode"

	local slot = self.slots[event]
	if not slot then
		slot = handlerSlot.new()
		self.slots[event] = slot
	end

	return slot:ReplaceHandler(handler,mutexMode)
end

function eventModule:Clear(event)
	local slot = self.slots[event]
	if slot then
		slot:Clear()
	end	
end

function eventModule:Emit(event,...)
	local slot = self.slots[event]
	if slot then
		slot:OnEvent(...)
	end
end

return M