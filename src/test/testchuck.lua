local chuck = require("chuck")

print("-----------chuck----------------")
for k,v in pairs(chuck) do
	print(k,v)
end

print("-----------chuck.socket---------------")
for k,v in pairs(chuck.socket) do
	print(k,v)
end

print("-----------chuck.socket.stream-------------")
for k,v in pairs(chuck.socket.stream) do
	print(k,v)
end

print("-----------chuck.socket.stream.ip4-------------")
for k,v in pairs(chuck.socket.stream.ip4) do
	print(k,v)
end