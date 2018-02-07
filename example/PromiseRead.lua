package.cpath = './lib/?.so;'
package.path = './lib/?.lua;'

local chuck = require("chuck")
local event_loop = chuck.event_loop.New()
local PromiseConnection = require("PromiseConnection").init(event_loop)
local strpack = string.pack
local strunpack = string.unpack

local function _set_byte2(n)
    return strpack(">I2", n)
end

local function _get_byte2(data, i)
    return strunpack(">I2", data, i)
end

local server = PromiseConnection.listen("127.0.0.1",9010,function (conn)

	print("newclient",conn)

	conn:SetCloseCallBack(function (err)
		print("client disconnected:",err)
	end)

	local function recv()
		conn:Recv(2):andThen(function (msg)
			local len = _get_byte2(msg,1)
			return conn:Recv(len)
		end):andThen(function (msg)
			print(msg)
			conn:Send(_set_byte2(#"hello world"))
			conn:Send("hello world")
			recv()
		end):catch(function (err)
			print(err)
		end)
	end
	recv()
end)

if server then
	event_loop:WatchSignal(chuck.signal.SIGINT,function()
		event_loop:Stop()
	end)	
	event_loop:Run()
end