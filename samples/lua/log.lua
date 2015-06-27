local chuck  = require("chuck")

local log       = chuck.log

local testlog = log.LogFile("test")

testlog:Log(log.INFO,"hello\n")
