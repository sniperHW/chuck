local Task   = require("distri.uthread.task")
local Distri = require("distri.distri")
local Redis  = require("distri.redis")
local chuck  = require("chuck")


local count  = 0

local err,client = Redis.Connect("127.0.0.1",6379)

if client then
	for i = 1,1000 do
		Task.New(function ()
				local cmd = string.format("hmget chaid:%d chainfo skills",i)
				while true do	
					local err,reply = client:Do(cmd)
					if reply then
						count = count + 1
					end
				end
		end)
	end

	local last = chuck.systick()
	Distri.RegTimer(1000,function ()
   		collectgarbage("collect") 
   		local now = chuck.systick()
   		print("hmget:" .. count*1000/(now-last) .. "/s")
   		last = now
   		count = 0 
	end)
	Distri.Signal(chuck.signal.SIGINT,Distri.Stop)
	Distri.Run()	
end



