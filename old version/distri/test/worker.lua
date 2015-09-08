local chuck = require("chuck")
local socket = require("distri.socket")
local engine = require("distri.engine")
local clone   = chuck.packet.clone
local cthread = chuck.cthread
local Distri = require("distri.distri")

cthread.process_mail(engine,function (sender,mail)
	local fd = table.unpack(mail)
	local s = socket.stream.New(fd)
	if s:Ok(4096,socket.stream.decoder.rawpacket(),function (_,msg,errno)
		if msg then
			s:Send(clone(msg))
		else
			s:Close(errno)
			s = nil
		end
	end) then
		s:SetRecvTimeout(5000)
	else
		s:Close()
	end	
end)
	
Distri.Run()
