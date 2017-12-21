package.path = './lib/?.lua;'

local event = require("eventHandler").new()

local handler1 = event:RegisterHandler("event1","queueMode",function ()
	print("event1 handler 1")
end)

local handler2 = event:RegisterHandler("event1","queueMode",function ()
	print("event1 handler 2")
end)

local handler3 = event:RegisterHandler("event1","queueMode",function ()
	print("event1 handler 3")
end)

event:Emit("event1")

print(handler2:UnRegister())

print("after handler2:UnRegister()")

event:Emit("event1")

local handler4 = event:RegisterHandler("event1","mutexMode",function ()
	print("event1 handler 4")
end)

local handler5 = event:RegisterHandler("event1","mutexMode",function ()
	print("event1 handler 5")
	return "unregister" --调用后被反注册
end)

print("mutexMode")

event:Emit("event1")

print("mutexMode after handler5:UnRegister()")

event:Emit("event1")

print("after handler4:UnRegister()")

print(handler4:UnRegister())

event:Emit("event1")

local handler6 = event:ReplaceHandler("event1","mutexMode",function ()
	print("event1 handler 6")
end)

print("after ReplaceHandler()")

event:Emit("event1")