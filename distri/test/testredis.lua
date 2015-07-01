local Task   = require("distri.uthread.task")
local Distri = require("distri.distri")
local Redis  = require("distri.redis")
local Socket = require("distri.socket")
local chuck  = require("chuck")
local Packet = chuck.packet
local clone  = Packet.clone

local err,client = Redis.Connect("127.0.0.1",6379)

if client then
	for i = 1,1000 do
		Task.New(function ()
			local cmd = string.format("hmset chaid:%d chainfo %s skills %s",i,
									  "fasdfasfasdfasdfasdfasfasdfasfasdfdsaf",
									  "fasfasdfasdfasfasdfasdfasdfcvavasdfasdf")		
			local err,reply = client:Do(cmd)
			if reply then
				print(i,reply)
			end
		end)
	end

	local server = Socket.stream.listen("127.0.0.1",8010,function (s,errno)
		if not s then
			return
		end
		local succ = s:Ok(4096,Socket.stream.rawdecoder,Task.Wrap(function (_,msg,errno)
			if msg then						
				local cmd = string.format("hmget chaid:%d chainfo skills",1)	
				local err,reply = client:Do(cmd)
				local result = ""
				if reply then
					for k,v in pairs(reply) do
						result = result .. v .. "\n"
					end
				else
					result = "error\n"
				end					
				s:Send(Packet.rawpacket(result))
			else
				s:Close()
				s = nil
			end
		end))
		if succ then	
			s:SetRecvTimeout(5000)
		end	
	end)
	if server then
		Distri.Signal(chuck.signal.SIGINT,Distri.Stop)
		Distri.Run()
	end
end



