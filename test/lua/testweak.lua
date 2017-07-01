package.path = './lib/?.lua;'

local weak = require("weak")

local obj = {}

setmetatable(obj,
	{
		__gc = function () 
			print("gc") 
		end,
		__call = function (self,arg1,arg2)
			local isWeakRef = self.isWeakRef
			self = self.isWeakRef and self.refObj or self
			print("_call",arg1,arg2,self,obj)
			if isWeakRef then
				return "call from weakRef"
			else
				return "call from originalObj"
			end
		end,
		__len = function (self)
			print("__len",self)
			return 0
		end
	})

function obj:a()
	self:funb()
end

function obj:funb()
	print("funb")
end

local ref = weak.Ref(obj)

ref.b = 1
ref:a()
print(#ref)
print(ref.b,obj.b)
print(obj(3,4))
print(ref(1,2))

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


local array = {1,2,3,4}

ref = weak.Ref(array)

for v in ipairs(ref) do
	print(v)
end

print(#ref)

ref = weak.Ref(1)