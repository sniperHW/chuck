package.path = './lib/?.lua;'

local weak = require("weak")

local obj = {}

setmetatable(obj,{__gc = function () print("gc") end})

function obj:a()
	print(self.b)
	self:funb()
end

function obj:funb()
	print("funb")
end

local ref = weak.Ref(obj)

ref.b = 1
ref:a()
print(ref.b,obj.b)

for k,v in pairs(ref) do
	print(k,v)
end

obj = nil
print("before gc")
collectgarbage("collect")
print("after gc")


local obj1 = {}

setmetatable(obj1,{__gc = function () print("gc1") end})


local weakVTable = weak.ValueTable()
weakVTable[1] = obj1
weakVTable["hello"] = "world"

for k,v in pairs(weakVTable) do
	print(k,v)
end 

obj1 = nil
print("before gc")
collectgarbage("collect")
print("after gc")