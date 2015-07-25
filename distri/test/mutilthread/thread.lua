local chuck = require("chuck")
local socket = require("distri.socket")
local engine = require("distri.engine")
local clone   = chuck.packet.clone
local cthread = chuck.cthread
local Distri = require("distri.distri")

print("thread running " .. cthread.selftid())

local mainthread

cthread.process_mail(engine,function (sender,mail)
	if mail.broadcast then
		--测试广播
		print("i'm " .. cthread.selftid() .. ",i got a broadcast mail")
	elseif mail.tid then
		--测试通过tid获取线程标识
		mainthread = cthread.findbytid(mail.tid)
		if mainthread then
			print("ok got mainthread tid",mail.tid)
		end
	elseif mainthread then	
		local fd  = mail[1]
		local msg = mail[2]
		msg = string.upper(msg)
		--只是为了测试,实际可以直接使用sender
		cthread.sendmail(mainthread,{fd,msg})
	end
end)
	
Distri.Run()
