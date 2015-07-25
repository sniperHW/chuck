local chuck = require("chuck")
local engine = require("distri.engine")
local Distri = require("distri.distri")
local cthread = chuck.cthread
local socket = require("distri.socket")

local worker
local fd2Socket = {}

--注册邮件处理函数，用于处理工作线程返回的消息
cthread.process_mail(engine,function (sender,mail)
	local fd = mail[1]
	local s  = fd2Socket[fd]
	if s then
		s:Send(chuck.packet.rawpacket(mail[2]))
	end 	
end)

local server = socket.stream.Listen("127.0.0.1",8010,function (s,errno)
	if s then
		if s:Ok(4096,socket.stream.decoder.rawpacket(),function (_,msg,errno)
			if msg then
				--将消息发送给工作线程处理
				cthread.sendmail(worker,{s:Fd(),msg:ReadBin()})
			else
				fd2Socket[s:Fd()] = nil
				s:Close(errno)
				s = nil
			end
		end) then
			fd2Socket[s:Fd()] = s
		end
	end
end)

if server then	
	Distri.Signal(chuck.signal.SIGINT,Distri.Stop)
	local t = Distri.RegTimer(1000,function() 
		collectgarbage("collect")	
	end)
	worker = cthread.new("distri/test/mutilthread/thread.lua")
	--将自己的tid发送给worker
	cthread.sendmail(worker,{tid = cthread.selftid()})
	engine:Run()
end