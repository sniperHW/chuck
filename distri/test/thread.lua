local chuck = require("chuck")
local cthread = chuck.cthread
local log   = chuck.log
local l     = log.LogFile("hello")

l:Log(log.INFO,"parent")

local t = cthread.new("distri/test/hello.lua")
cthread.sendmail(t,{"hello world!"})
cthread.join(t)
