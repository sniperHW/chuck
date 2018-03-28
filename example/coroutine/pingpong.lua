package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local coroutine = require("ccoroutine")
local CoroSocket = require("CoroSocket").init(event_loop)
local server

local content = ""
for i = 1,511 do
	content = content .. "111111111"
end

local contentLen = #content
print(contentLen)

local function startServer()

	local client_count = 0
	local packet_count = 0
	local bytes = 0
	local lastShow = chuck.time.systick()

	server = CoroSocket.ListenIP4("127.0.0.1",8010,function(socket,err)
		if socket then
			client_count = client_count + 1
			while true do
				local msg,err = socket:Recv(contentLen)
				if msg then
					packet_count = packet_count + 1
					bytes = bytes + #msg
					local now = chuck.time.systick()
					local delta = now - lastShow
					if delta >= 1000 then
						lastShow = now
						print(string.format("clients:%d,pkt:%.0f/s,bytes:%.2fMB/s",client_count,packet_count*1000/delta,(bytes/1024/1024)*1000/delta))
						packet_count = 0
						bytes = 0
						collectgarbage("collect")
					end
					socket:Send(msg)
				else
					socket:Close()
					print("connection lose")
					break
				end
			end
			client_count = client_count - 1
		end
	end)
	return server
end


local function startClient(count)
	for i = 1,count do
		coroutine.run(function ()
			local socket = CoroSocket.ConnectIP4("127.0.0.1",8010)
			if socket then
				socket:Send(content)
				while true do
					local msg,err = socket:Recv(contentLen)
					if err then
						break
					end
					socket:Send(content)
				end
			end
		end)
	end
end

local function run()

	local runType = arg[1]
	if not runType then
		print("usage: lua benchmark_brocast both|server|client [clientCount]")
		return
	end
	local clientCount = arg[2]


	if (runType == "both" or runType == "client") and nil == clientCount then
		print("usage: lua benchmark_brocast both|server|client [clientCount]")
		return
	end	

	if runType == "both" or runType == "server" then
		if not startServer() then
			print("start server failed")
			return
		end
	end

	if runType == "both" or runType == "client" then
		startClient(clientCount)
	end

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		event_loop:Stop()
	end)
	event_loop:Run()
end

run()