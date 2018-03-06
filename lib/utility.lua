local M = {}

M.logger = print

function M.setLogger(logger)
	assert(logger == "function","logger must be a function")
	M.logger = logger or print
end

function M.pcall(func,...)
	local ret = table.pack(xpcall(func,function (err)
		M.logger(err)
	end,...))

	if ret[1] then
		return select(2,table.unpack(ret))
	else
		return nil
	end
end

return M