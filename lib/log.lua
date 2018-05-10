local chuck = require("chuck")
local log = chuck.log

local M = {}

local global_loglevel = log.GetLogLevel()

function M.SetSysLogPrefix(prefix)
	log.SetSysLogPrefix(prefix)
end

function M.SetLogDir(dir)
	log.SetLogDir(dir)
end


local str2Lev = {
	["trace"]    = log.trace,
	["debug"]    = log.debug,
	["info"]     = log.info,
	["warning"]  = log.warning,
	["error"]    = log.error,
	["critical"] = log.critical				
}

local lev2Str = {
	[log.trace]    = "trace",
	[log.debug]    = "debug",
	[log.info]     = "info",
	[log.warning]  = "warning",
	[log.error]    = "error",
	[log.critical] = "critical",					
}

function M.GetLogLevel()
	return lev2Str[global_loglevel]
end

function M.SetLogLevel(level)
	level = str2Lev[level]
	if level and log.SetLogLevel(level) then
		global_loglevel = level
	end
end

local logger = {}
logger.__index = logger

local function logger_new(name)
	local o = setmetatable({},logger)
	o.clogger = log.CreateLogfile(name)
	return o
end

function logger:trace(...)
	if global_loglevel <=  log.trace then
		self.clogger:Log(log.trace,string.format(...))
	end
end

function logger:debug(...)
	if global_loglevel <=  log.debug then
		self.clogger:Log(log.debug,string.format(...))
	end
end

function logger:info(...)
	if global_loglevel <=  log.info then
		self.clogger:Log(log.info,string.format(...))
	end
end

function logger:warning(...)
	if global_loglevel <=  log.warning then
		self.clogger:Log(log.warning,string.format(...))
	end
end

function logger:error(...)
	if global_loglevel <=  log.error then
		self.clogger:Log(log.error,string.format(...))
	end
end

function logger:critical(...)
	if global_loglevel <=  log.critical then
		self.clogger:Log(log.critical,string.format(...))
	end
end

local syslogger = {}
syslogger.__index = syslogger
syslogger = setmetatable(syslogger,syslogger)

function M.SysLogger()
	return syslogger
end

function syslogger:trace(...)
	if global_loglevel <=  log.trace then
		log.SysLog(log.trace,string.format(...))
	end
end

function syslogger:debug(...)
	if global_loglevel <=  log.debug then
		log.SysLog(log.debug,string.format(...))
	end
end

function syslogger:info(...)
	if global_loglevel <=  log.info then
		log.SysLog(log.info,string.format(...))
	end
end

function syslogger:warning(...)
	if global_loglevel <=  log.warning then
		log.SysLog(log.warning,string.format(...))
	end
end

function syslogger:error(...)
	if global_loglevel <=  log.error then
		log.SysLog(log.error,string.format(...))
	end
end

function syslogger:critical(...)
	if global_loglevel <=  log.critical then
		log.SysLog(log.critical,string.format(...))
	end
end

function M.new(name)
	return logger_new(name)
end

M.SetLogLevel("debug")

return M