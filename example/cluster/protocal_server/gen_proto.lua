protos = {}

function protocal(name,id)
	protos[name] = id
end

require("proto_define")

for k,v in pairs(protos) do
	local f = io.open("./proto/" .. k .. ".proto","r")
	if nil == f then
		f = io.open("./proto/" .. k .. ".proto","a")
		if nil == f then
			error("create " .. " failed")
		else
			f:write('syntax = "proto2";\n')
			f:write('package chuck;\n')
			f:flush()
			f:close()
		end
	end 
end