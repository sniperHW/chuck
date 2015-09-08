local chuck  = require("chuck")
local log    = chuck.log
local daemon = chuck.daemon

daemon.daemonize()

log.SysLog(log.ERROR,"i'm a deaemon")
