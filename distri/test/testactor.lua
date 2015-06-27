local Sche = require "distri.sche"
local Actor = require "distri.actor"

for i=1,10 do
	Actor.Spawn(i,function (self)
		while true do
			local msg = self:Recv()
			print(msg,self.identity)
		end	
	end)
end

while true do
	--test boardcast
	Actor.BoardCast("hello",{1,3,5,7,9})
	Sche.Schedule()
end	
