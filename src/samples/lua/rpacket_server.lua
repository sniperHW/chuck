local chuck = require("chuck")
local socket_helper = chuck.socket_helper
local acceptor = chuck.acceptor
local connection = chuck.connection
local packet = chuck.packet
local decoder = chuck.decoder
local signal  = chuck.signal

local fd =  socket_helper.socket(socket_helper.AF_INET,
								 socket_helper.SOCK_STREAM,
								 socket_helper.IPPROTO_TCP)
socket_helper.addr_reuse(fd,1)

local engine
local packetcout = 0

function on_packet(conn,p,event)
	if event == "RECV" then
		packetcout = packetcout + 1
		conn:Send(packet.wpacket(p))
	end
end

local clients = {}

function on_new_client(fd)
	print("on_new_client")
	local conn = connection(fd,4096,decoder.rpacket(4096))
	conn:Add2Engine(engine,on_packet)
	conn:SetDisConCb(function () 
					  print("conn disconnect")
					  for k,v in pairs(clients) do
					  	if v == conn then
					  		table.remove(clients,k)
					  		return
					  	end
					  end
					 end)
	table.insert(clients,conn)--hold the conn to prevent lua gc
end

local ip = "127.0.0.1"
local port = 8010

local function sigint_handler()
	print("recv sigint")
	engine:Stop()
end

local signaler = signal.signaler(signal.SIGINT)

if 0 == socket_helper.listen(fd,ip,port) then
	print("server start",ip,port)
	print("server start")
	local server = acceptor(fd)
	engine = chuck.engine()
	server:Add2Engine(engine,on_new_client)
	chuck.RegTimer(engine,1000,function() 
		collectgarbage("collect")
		print(packetcout,chuck.get_bytebuffer_count())
		packetcout = 0		
	end)
	signaler:Register(engine,sigint_handler)
	engine:Run()
end
