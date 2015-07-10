local chuck   = require("chuck")
local process = chuck.process

print(process.fork("/usr/local/bin/lua","./distri/test/child.lua"))
