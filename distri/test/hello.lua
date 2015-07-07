local chuck  = require("chuck")
local cthread = chuck.cthread
local log    = chuck.log
local l      = log.LogFile("hello")
local engine = require("distri.engine")

l:Log(log.INFO,"child")

cthread.process_mail(engine,function (sender,mail)
	print(table.unpack(mail))
	engine:Stop()
end)

engine:Run()


