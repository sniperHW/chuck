local chuck = require("chuck")
local socket_helper = chuck.socket_helper
local connection = chuck.connection
local packet = chuck.packet

local engine

local function sigint_handler()
	print("recv sigint")
	engine:Stop()
end

local signaler = signal.signaler(signal.SIGINT)

function on_packet(conn,p,event)
	if event == "RECV" then
		conn:Send(packet.clone(p),"notify")
	elseif event == "SEND" then
		print("send finish")
		--conn:Close()
	end
end

function on_new_client(fd)
	print("on new client\n")
	local conn = connection(fd,4096)
	conn:Add2Engine(engine,on_packet)
	conn:SetDisConCb(function () 
					  print("conn disconnect")
					  conn = nil
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
	chuck.RegTimer(engine,1000,function() collectgarbage("collect") print("timeout") end)
	signaler:Register(engine,sigint_handler)
	engine:Run()
end
