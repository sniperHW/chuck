local chuck = require("chuck")


for k,v in pairs(chuck) do
	print(k,v)
end

for k,v in pairs(chuck.packet) do
	print(k,v)
end


print(chuck.systick())