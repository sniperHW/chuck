package.path = './lib/?.lua;'

local utility = require("utility")

print(utility.pcall(function ()
	return 1,2
end))

print(utility.pcall(function ()
	a.a = 1 
	return 1,2
end))