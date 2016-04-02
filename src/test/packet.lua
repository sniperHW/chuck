local chuck = require("chuck")
local packet = chuck.packet

local buff = chuck.buffer.New()
local w = packet.Writer(buff)
w:WriteI8(1)
w:WriteI16(-2)
w:WriteNum(3.2)
w:WriteStr("hello")
w:WriteStr("fasdfasdfasffdsafjklafklkfjklfjlkasfjaklfjfkjasfjaklf")
w:WriteStr("fasdfasdfasffdsafjklafklkfjklfjlkasfjaklfjfkjasfjaklf")
--w:WriteTable({-2,3,6273549})
w:WriteTable({a=1,b=2.8})

local r = packet.Reader(buff)

print(r:ReadI8())
print(r:ReadI16())
print(r:ReadNum())
print(r:ReadStr())
print(r:ReadStr())
print(r:ReadStr())
local t = r:ReadTable()
--for k,v in pairs(t) do
--	print(v)
--end
--local t = r:ReadTable()
print(t.a,t.b)
