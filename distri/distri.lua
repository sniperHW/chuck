local engine = require("distri.engine")
local Sche = require("distri.uthread.sche")

local distri = {}

function distri.Run()
	local ms = 1
	while engine:Run(ms) do
		ms = Sche.Schedule() > 0 and 1 or 0
	end
end

return distri