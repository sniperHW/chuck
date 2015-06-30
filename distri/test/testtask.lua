local Sche = require("distri.uthread.sche")
local Task = require("distri.uthread.task")
local LinkQue = require("distri.linkque")

for i = 1,10 do
	Task.New(function ()
		Sche.Sleep(100)
		print("i'm",i,Sche.Running())
	end)
end

while true do
	Sche.Schedule()
end 

