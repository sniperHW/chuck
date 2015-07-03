local RPC    = require("distri.rpc")
local chuck  = require("chuck")
local Packet = chuck.packet
local config = RPC.Config(
function (data)                           --encoder                        
	local wpk = Packet.wpacket(512)
	wpk:WriteTab(data)
	return wpk
end,
function (packet)                         --decoder
	return packet:ReadTab()
end)
return config