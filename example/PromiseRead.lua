package.cpath = './lib/?.so;'
package.path = './lib/?.lua;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local PromiseSocket = require("PromiseSocket").init(event_loop)
local strpack = string.pack
local strunpack = string.unpack

local function _set_byte2(n)
    return strpack(">I2", n)
end

local function _get_byte2(data, i)
    return strunpack(">I2", data, i)
end

local packet_count = 0
local lastShow = chuck.time.systick()

local server = PromiseSocket.listen("127.0.0.1",9010,function (conn)

	conn:OnClose(function ()
		print("client disconnected")
	end)

	local function recv()
		conn:Recv(2):andThen(function (msg)
			local len = _get_byte2(msg,1)
			return conn:Recv(len)
		end):andThen(function (msg)
			conn:Send(_set_byte2(#"hello world") .. "hello world")
			packet_count = packet_count + 1
			local now = chuck.time.systick()
			local delta = now - lastShow
			if delta >= 1000 then
				lastShow = now
				print(string.format("pkt:%.0f/s",packet_count*1000/delta))
				packet_count = 0
				collectgarbage("collect")
			end
			recv()
		end):catch(function (err)
			print("recv error",err)
			conn:Close()
		end)
	end
	recv()
end)

if server then
	for i=1,arg[1] do
		PromiseSocket.connect("127.0.0.1",9010):andThen(function (conn)
			conn:OnClose(function ()
				print("disconnected")
			end)

			local function send()
				conn:Send(_set_byte2(#"hi") .. "hi")
			end

			local function recv()
				conn:Recv(2):andThen(function (msg)
					local len = _get_byte2(msg,1)
					return conn:Recv(len)
				end):andThen(function (msg)
					send()
					recv()
				end):catch(function (err)
					print(err)
				end)
			end
			send()
			recv()
		end):catch(function (err)
			print(err)
		end)
	end

	event_loop:AddTimer(1000,function ()
		collectgarbage("collect")
	end)

	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		event_loop:Stop()
	end)	
	event_loop:Run()
end