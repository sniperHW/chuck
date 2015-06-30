local chuck = require("chuck")
local socket_helper = chuck.socket_helper

local host = socket_helper.gethostbyname_ipv4("www.baidu.com")
if host then
	for k,v in pairs(host) do
		print(v)
	end
end
