local chuck = require("chuck")
local packet = require("packet")

local buff = chuck.buffer.New()
local w = packet.Writer(buff)
w:WriteI8(1)
w:WriteI16(2)
w:WriteI32(3)

local r = packet.Reader(buff)

print(r:ReadI8())
print(r:ReadI16())
print(r:ReadI32())