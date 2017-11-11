package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local packet = chuck.packet
local buffer = chuck.buffer


local b = buffer.New()
local writer = packet.Writer(b)
writer:WriteTable({a="",b=1})

local reader = packet.Reader(b)
local tt = reader:ReadTable()
print(tt.a,tt.b)