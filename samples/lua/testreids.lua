local chuck = require("chuck")
local socket_helper = chuck.socket_helper
local connection = chuck.connection
local packet = chuck.packet
local err = chuck.error

function on_packet(conn,p,event)
	if event == "RECV" then
		print(p:ReadBin())
		--conn:Send(packet.wpacket(p))
	end
end

local engine = chuck.engine()


function connect_callback(fd,errnum)
	if errnum ~= 0 then
		socket_helper.close(fd)
	else
		print("connect success")
		local conn = connection(fd,4096)
		conn:Add2Engine(engine,on_packet)
		conn:SetDisConCb(function () 
					  		print("conn disconnect")
					  		conn = nil
					 	 end)
		--local wpk = packet.rawpacket("*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n")
		local wpk = packet.rawpacket("*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n")
		conn:Send(wpk)		
	end	
end

for i = 1,1 do
	local fd =  socket_helper.socket(socket_helper.AF_INET,
									 socket_helper.SOCK_STREAM,
									 socket_helper.IPPROTO_TCP)

	socket_helper.noblock(fd,1)

	local ret = socket_helper.connect(fd,"127.0.0.1",6379)

	if ret == 0 then
		connect_callback(fd,0)
	elseif ret == -err.EINPROGRESS then
		local connector = chuck.connector(fd,5000)
		connector:Add2Engine(engine,function(fd,errnum)
										connector = nil 
										--use closure to hold the reference of connector
										connect_callback(fd,errnum)
									end)
	else
		print("connect to 127.0.0.1 8010 error")
	end
end

chuck.RegTimer(engine,1000,function() 
	collectgarbage("collect")
end)

engine:Run()
