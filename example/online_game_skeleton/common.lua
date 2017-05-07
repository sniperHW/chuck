package.cpath = './lib/?.so;'
package.path = './lib/?.lua;./example/online_game_skeleton/db/?.lua;./server/newserver/protocal/?.lua;./example/online_game_skeleton/gameserver/?.lua;'
package.path = package.path .. './example/online_game_skeleton/gateserver/?.lua;./example/online_game_skeleton/roomserver/?.lua;./example/online_game_skeleton/common/?.lua;' 

local chuck = require("chuck")
local rpc = require("rpc")
local buffer = chuck.buffer
local netcmd = require("netcmd_define")
local packet = chuck.packet

rpc.pack = function(rpcmsg)
	local buff = buffer.New(string.len(rpcmsg) + 8)
	local writer = packet.Writer(buff)
	writer:WriteI16(netcmd.CMD_RPC)
	writer:WriteStr(rpcmsg)
	return buff
end