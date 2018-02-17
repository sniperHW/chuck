package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")

print(chuck.base64.encode(chuck.crypt.sha1("Er+PNTJDnDT4G+VX8V/1GA==" .. "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")))


--print(chuck.base64.encode("Er+PNTJDnDT4G+VX8V/1GA==" .. "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))

--print(chuck.crypt.sha1("Er+PNTJDnDT4G+VX8V/1GA==" .. "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
