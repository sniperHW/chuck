local chuck = require("chuck")
local engine = require("distri.engine")
local socket_helper = chuck.socket_helper
local Distri = require("distri.distri")
local cthread = chuck.cthread

local worker 

function on_new_client(fd)
	cthread.sendmail(worker,{fd})
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
	server:Add2Engine(engine,on_new_client)
	Distri.Signal(chuck.signal.SIGINT,Distri.Stop)	
	worker = cthread.new("distri/test/worker.lua")
	Distri.Run()
end

