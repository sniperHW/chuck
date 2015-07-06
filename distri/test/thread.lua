local chuck = require("chuck")
local cthread = chuck.cthread
local log   = chuck.log
local l     = log.LogFile("hello")

l:Log(log.INFO,"parent")

cthread.join(cthread.new("distri/test/hello.lua"))
