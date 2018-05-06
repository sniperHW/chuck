package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local log = require("log")

log.SetLogDir("log/test")
log.SetLogLevel("info")

local logger = log.new("test")

local sysLogger = log.SysLogger()


logger:debug("this is debug log")
logger:info("this is info log")
logger:error("this is error log")


sysLogger:info("this is info sys log")






