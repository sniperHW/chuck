local chuck = require("chuck")
local socket_helper = chuck.socket_helper
local acceptor = chuck.acceptor
local connection = chuck.connection
local packet = chuck.packet

local fd =  socket_helper.socket(socket_helper.AF_INET,
								 socket_helper.SOCK_STREAM,
								 socket_helper.IPPROTO_TCP)
socket_helper.addr_reuse(fd,1)

local engine

function on_packet(conn,p,event)
	if event == "RECV" then
		conn:Send(packet.rawpacket(p))
	end
end


function on_new_client(fd)
	local conn = connection(fd,4096)
	conn:Add2Engine(engine,on_packet)
end

if 0 == socket_helper.listen(fd,"127.0.0.1",8010) then
	print("server start")
	local server = acceptor(fd)
	engine = chuck.engine()
	server:Add2Engine(engine,on_new_client)
	engine:Run()
end
