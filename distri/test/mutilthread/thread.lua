local chuck = require("chuck")
local socket = require("distri.socket")
local engine = require("distri.engine")
local clone   = chuck.packet.clone
local cthread = chuck.cthread
local Distri = require("distri.distri")

cthread.process_mail(engine,function (sender,mail)
	local fd  = mail[1]
	local msg = mail[2]
	msg = string.upper(msg)
	cthread.sendmail(sender,{fd,msg})
end)
	
Distri.Run()
