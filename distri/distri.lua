local engine = require("distri.engine")
local Sche   = require("distri.uthread.sche")
local chuck  = require("chuck")
local signal = chuck.signal

local distri   = {}
local signaler = {}

--启动事件主循环
function distri.Run()
	chuck.RegTimer(engine,1,function()
		Sche.Schedule()
	end)
	engine:Run()
end

--[[注册定时器,成功返回一个定时器对象
    cb(),定时器到达时回调.如果希望定时器按之前设定的间隔再次触发则cb内部无需返回值
    如果希望终止定时器,则可以在cb内部return -1
    如果希望修改定时间隔,return 新间隔
    定时器支持的最大间隔为(1000*3600*24-1)毫秒
]]
function distri.RegTimer(ms,cb)
	return chuck.RegTimer(engine,ms,cb)
end

--删除定时器对象,如果在cb中return -1,定时器会被自动销毁不需要再手动调用RemTimer
function distri.RemTimer(t)
	return chuck.RemTimer(t)
end

--注册信号回调函数
function distri.Signal(sig,handler)
	local success = false
	local s = signaler[sig]
	if not s then
		s = {signal.signaler(sig),handler}
		success = s[1]:Register(engine,function (signo)
			s[2](signo)
		end)
		if success then
			signaler[sig] = s
			s[2] = handler
		end
	end
	return success
end

--终止消息循环,导致distri.Run返回
function distri.Stop()
	print("distri.Stop")
	engine:Stop()
end

return distri
