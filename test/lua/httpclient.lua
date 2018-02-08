package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local http = require("http").init(event_loop)
local socket = chuck.socket


local client = http.Client("127.0.0.1",8010)

client:Get("/",nil,function (response,err)
	if err then
		print(err)
		event_loop:Stop()
		return
	end
	for i,v in ipairs(response:AllHeaders()) do
		print(v[1],v[2])
	end	
	print(response:Body())
	client:Close()
	event_loop:Stop()
end)

event_loop:Run()

