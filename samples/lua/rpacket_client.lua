local chuck = require("chuck")
local socket_helper = chuck.socket_helper
local connection = chuck.connection
local packet = chuck.packet
local decoder = chuck.decoder
local errno = chuck.error
local signal = chuck.signal


local engine = chuck.engine()


local function sigint_handler()
	print("recv sigint")
	engine:Stop()
end

local signaler = signal.signaler(signal.SIGINT)


function connect_callback(fd,errnum)
	if errnum ~= 0 then
		socket_helper.close(fd)
	else
		print("connect success2")
		local conn = connection(fd,4096,decoder.connection.rpacket(4096))
		if conn then
			conn:Add2Engine(engine,function (_,p,err)
				print(p,err)
				if(p) then
					conn:Send(packet.wpacket(p))
				else
					conn:Close()
					conn = nil
				end
			end)

			local wpk = packet.wpacket(512)
			wpk:WriteTab({i=1,j=2,k=3,l=4,z=5})
			conn:Send(wpk)
		else
			print("create connection error")
		end		
	end	
end

for i = 1,100 do
	local fd =  socket_helper.socket(socket_helper.AF_INET,
									 socket_helper.SOCK_STREAM,
									 socket_helper.IPPROTO_TCP)

	socket_helper.noblock(fd,1)

	local ret = socket_helper.connect(fd,"127.0.0.1",8010)

	if ret == 0 then
		connect_callback(fd,0)
	elseif ret == -errno.EINPROGRESS then
		local connector = chuck.connector(fd,5000)
		connector:Add2Engine(engine,function(fd,errnum)
										--use closure to hold the reference of connector
										connect_callback(fd,errnum)
										connector = nil 
									end)
	else
		print("connect to 127.0.0.1 8010 error")
	end
end

chuck.RegTimer(engine,1000,function() 
	collectgarbage("collect")
end)

signaler:Register(engine,sigint_handler)
engine:Run()
