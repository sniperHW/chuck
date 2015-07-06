local chuck = require("chuck")
local log   = chuck.log
local l     = log.LogFile("hello")

l:Log(log.INFO,"child")

print("hello")