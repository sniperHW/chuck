package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local protobuf = require "protobuf"
local dump = require("dump")
local event_loop = chuck.event_loop.New()
local readline = require("readline")
local PromiseSocket = require("PromiseSocket").init(event_loop)
local strpack = string.pack
local strunpack = string.unpack

local addr = io.open("flyfish/proto.pb","rb")
local pb_buffer = addr:read "*a"
addr:close()
protobuf.register(pb_buffer)


local ip
local port

local CmdToProto = {
	[3] = "proto.set_req",
	[4] = "proto.set_resp",
	[5] = "proto.get_req",
	[6] = "proto.get_resp",
	[9] = "proto.del_req",
	[10] = "proto.del_resp",
	[11] = "proto.incr_by_req",
	[12] = "proto.incr_by_resp",
	[13] = "proto.decr_by_req",
	[14] = "proto.decr_by_resp",
	[15] = "proto.set_nx_req",
	[16] = "proto.set_nx_resp",
	[17] = "proto.compare_and_set_req",
	[18] = "proto.compare_and_set_resp",	
	[19] = "proto.compare_and_set_nx_req",
	[20] = "proto.compare_and_set_nx_resp",	
}

local function _set_byte2(n)
    return strpack(">I2", n)
end

local function _get_byte2(data, i)
    return strunpack(">I2", data, i)
end

local function _set_byte4(n)
    return strpack(">I4", n)
end

local function _get_byte4(data, i)
    return strunpack(">I4", data, i)
end


local function pack(cmd,data) 
	local code = protobuf.encode(CmdToProto[cmd], data)
	local len = 2 + #code
	return _set_byte4(len) .. _set_byte2(cmd) .. code 
end

local function unpack(buff)
	local cmd = _get_byte2(buff,1)
	return protobuf.decode(CmdToProto[cmd] , string.sub(buff,3))
end

local function doCmd(cmd)

	local ok

	PromiseSocket.connect(ip,port,5000):andThen(function (conn)
		conn:OnClose(function ()
			ok = true
		end)
		conn:Send(pack(cmd.cmd,cmd.req))
		conn:Recv(4,5000):andThen(function (msg)
			local len = _get_byte4(msg,1)
			conn:Recv(len,5000):andThen(function (msg)
				dump.print(unpack(msg),"resp",10)
			end)
			conn:Close()
		end):catch(function (err)
			print(err)
			conn:Close()
		end)
	end):catch(function (err)
		print(err)
		ok = true
	end)

	while not ok do
		event_loop:Run(10)
	end
end


function String(name,value)
	return {
		name = name,
		v = {type='string',s=value},
	}	
end

function Int(name,value)
	return {
		name = name,
		v = {type='int',i=value},
	}	
end

function Uint(name,value)
	return {
		name = name,
		v = {type='uint',u=value},
	}	
end

function Float(name,value)
	return {
		name = name,
		v = {type='float',f=value},
	}	
end

function Blob(name,value)
	return {
		name = name,
		v = {type='blob',b=value},
	}	
end

function get(table,key,fields)
	local cmd = {
		cmd = 5,
		req = {
			head = {
				seqno = 1,
				table = table,
				key = key,
				timeout = 5000000000,
			},
			fields = fields,
		}		
	}
	doCmd(cmd)
end

--set("game_user","huangwei",{String('userdata','{"Name":"huangwei","Level":2}')})
function set(table,key,fields,version)
	local cmd = {
		cmd = 3,
		req = {
			head = {
				seqno = 1,
				table = table,
				key = key,
				timeout = 5000000000,
			},
			version = version,
			fields = fields,
		}		
	}
	doCmd(cmd)	
end

local function execute_chunk(str)
	local func,err = load(str)
	if func then
		local ret,err = pcall(func)
		if not ret then
			print("command error:" .. err)
		end
	elseif err then
		print("command error:" .. err)
	end
end

local function repl()
	local chunk = ""

	local prompt = ">>"

	while true do
		local cmd_line = readline(prompt)
		if #cmd_line > 1 then
			if string.byte(cmd_line,#cmd_line) ~= 92 then
				chunk = chunk .. cmd_line
				break
			else
			  	chunk = chunk .. string.sub(cmd_line,1,#cmd_line-1) .. "\n"
				prompt = ">>>"
			end
		else
			break
		end	
	end

	if chunk ~= "" then
		if chunk == "exit" then
			return false
		else
			execute_chunk(chunk)
		end
	end

	return true
end

if arg == nil or #arg ~= 2 then
	print("useage:lua flyfish.lua ip port")
else
   ip,port = arg[1],arg[2]
   while true do
   		if not repl() then
   			return		
   		end	
   end
end


