
protos = {}

function protocal(name,id)
	protos[name] = id
end

require("proto_define")

local f = io.open("pb.lua","w+")
f:write(
[[
local protobuf = require "protobuf"
local M = {}
local name2id  = {}
local id2name  = {}
local loadfuns = {}

function M.getid(name)
	return name2id[name]
end

function M.getname(id)
	return id2name[id]
end

function M.init(p)
	p = p or "."
	for k,v in pairs(loadfuns) do
		v()
	end
end

function M.encode(...)
	protobuf.encode(...)
end

function M.decode(...)
	protobuf.decode(...)
end

]]
)


for k,v in pairs(protos) do
	f:write(string.format("----------%s--------------\n",k))
	f:write(string.format("name2id['%s']=%d\n",k,v))
	f:write(string.format("id2name[%d]='%s'\n",v,k))
	f:write(string.format(
[[table.insert(loadfuns,function ()
	local addr = io.open(path .. "/%s.pb","rb")
	local pb_buffer = addr:read "*a"
	addr:close()
	protobuf.register(pb_buffer)
end]],k))
	f:write("\n\n")
end

f:write("return M\n")

f:flush()
f:close()