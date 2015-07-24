local chuck = require("chuck")
local socket_helper = chuck.socket_helper
local connection = chuck.connection
local packet = chuck.packet
local signal = chuck.signal

local engine

local function sigint_handler()
	print("recv sigint")
	engine:Stop()
end

local signaler = signal.signaler(signal.SIGINT)


function on_new_client(fd)
	print("on new client\n")
	local conn = connection(fd,4096)
	conn:Add2Engine(engine,function (_,p,err)
		if p then
			p = packet.clone(p)
			print(p)
			conn:Send(p,function (_)
					  	print("packet send finish")
					  	conn:Close()
						conn = nil
					  end)
		else
			conn:Close()
			conn = nil
		end
	end)
end

local fd =  socket_helper.socket(socket_helper.AF_INET,
								 socket_helper.SOCK_STREAM,
								 socket_helper.IPPROTO_TCP)
socket_helper.addr_reuse(fd,1)


local ip = "127.0.0.1"
local port = 8010

if 0 == socket_helper.listen(fd,ip,port) then
	print("server start",ip,port)
	local server = chuck.acceptor(fd)
	engine = chuck.engine()
	server:Add2Engine(engine,on_new_client)
	local t = chuck.RegTimer(engine,1000,function() collectgarbage("collect") end)
	signaler:Register(engine,sigint_handler)
	engine:Run()
end
