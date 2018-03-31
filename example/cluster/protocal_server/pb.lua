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

----------Test2--------------
name2id['Test2']=4
id2name[4]='Test2'
table.insert(loadfuns,function ()
	local addr = io.open(path .. "/Test2.pb","rb")
	local pb_buffer = addr:read "*a"
	addr:close()
	protobuf.register(pb_buffer)
end

----------Test1--------------
name2id['Test1']=3
id2name[3]='Test1'
table.insert(loadfuns,function ()
	local addr = io.open(path .. "/Test1.pb","rb")
	local pb_buffer = addr:read "*a"
	addr:close()
	protobuf.register(pb_buffer)
end

----------ServiceList--------------
name2id['ServiceList']=2
id2name[2]='ServiceList'
table.insert(loadfuns,function ()
	local addr = io.open(path .. "/ServiceList.pb","rb")
	local pb_buffer = addr:read "*a"
	addr:close()
	protobuf.register(pb_buffer)
end

----------Auth--------------
name2id['Auth']=1
id2name[1]='Auth'
table.insert(loadfuns,function ()
	local addr = io.open(path .. "/Auth.pb","rb")
	local pb_buffer = addr:read "*a"
	addr:close()
	protobuf.register(pb_buffer)
end

return M
