package.path = './lib/?.lua;'

local util = require("util")

print(util.pcall(function ()
	return 1,2
end))

print(util.pcall(function ()
	a.a = 1 
	return 1,2
end))