local LinkQue =  require "distri.linkque"
local chuck   =  require("chuck")
local Log     =  chuck.log
local SysLog  =  Log.SysLog 

local sche = {
	ready      = LinkQue.New(),
	timer      =  chuck.TimingWheel(),
	allcos      = {},
	co_count = 0,
	current    = nil,
}

local stat_ready   = 1
local stat_sleep   = 2
local stat_yield   = 3
local stat_dead    = 4
local stat_block   = 5
local stat_running = 6

local function add2Ready(co)
    local status = co.status
    if status == stat_ready or status == stat_dead or status == stat_running then
    	return
    end
    co.status = stat_ready
    if co.wheel then
    	co.wheel:UnRegister()
    	co.wheel = nil
    end    
    sche.ready:Push({co}) 
end

local function block(ms,stat)
	local co = sche.runningco
	if co.status ~= stat_running then
		return
	end
    	if ms and ms > 0 then
			if co.wheel then	
    	    	co.wheel:UnRegister()
    	    end
    	    co.wheel = sche.timer:Register(ms,function ()
    	       	co.timeout = true
    	       	add2Ready(co)
    	    end)
    end
	co.status = stat
	coroutine.yield(co.coroutine)
	if co.timeout then	
    	co.timeout = nils		
	    return "timeout"
	end
end

local function SwitchTo(co)
	local pre_co = sche.runningco
	sche.runningco = co
	if co.wheel then	
    	       	co.wheel:UnRegister()
    	       	co.wheel = nil
    	end		
	co.status = stat_running
	coroutine.resume(co.coroutine,co)
	sche.runningco = pre_co
	return co.status	
end

local function Schedule(co)
	local readylist = sche.ready
	if co then
		local status = co.status
		if status == stat_ready or status == stat_dead or status == stat_running then
			return sche.ready:Len()
		end	
		if SwitchTo(co) == stat_yield then
			add2Ready(co)
		end
		if sche.stop then
			return -1
		end		
	else
		sche.timer:Tick()
		local yields = {}
		co = readylist:Pop()
		while co do
			co = co[1]
			if SwitchTo(co) == stat_yield then
				table.insert(yields,co)
			end
			if sche.stop then
				return -1
			end			
			co = readylist:Pop()
		end
		sche.timer:Tick()	
		for k,v in pairs(yields) do
			add2Ready(v)
		end
    end
    return sche.ready:Len()
end

local function start_fun(co)
	local stack,errmsg
    	if not xpcall(co.start_func,
    		       function (err)
    			errmsg = err
    			stack  = debug.traceback()
    		       end,
    		       table.unpack(co.args)
    	) then
        		SysLog(Log.ERROR,string.format("error on start_fun:%s\n%s",errmsg,stack))
    	end
    	sche.allcos[co.identity] = nil
    	sche.co_count = sche.co_count - 1
    	co.status = stat_dead
end

local g_counter = 0
local function gen_identity()
	g_counter = bit32.band(g_counter + 1,0x000FFFFF)
	return string.format("%d-%d",chuck.systick(),g_counter)
end

--产生一个coroutine在下次调用Schedule时执行
local function Spawn(func,...)
	local co = {index=0,timeout=0,identity=gen_identity(),start_func = func,args={...}}
	co.coroutine = coroutine.create(start_fun)
	sche.allcos[co.identity] = co
	sche.co_count = sche.co_count + 1
	add2Ready(co)
	return co
end

--产生一个coroutine立刻执行
local function SpawnAndRun(func,...)
	local co = {index=0,timeout=0,identity=gen_identity(),start_func = func,args={...}}
	co.coroutine = coroutine.create(start_fun)
	sche.allcos[co.identity] = co
	sche.co_count = sche.co_count + 1	
	Schedule(co)
	return co
end

local function Exit()
	sche.stop = true
	Yield() --yield to scheduler	
end

return {
	Spawn = Spawn,
	SpawnAndRun = SpawnAndRun,
	Yield =  function () block(0,stat_yield) end,
	Sleep = function (ms) block(ms,stat_sleep) end,
	Block = function (ms) block(ms,stat_block) end,
	WakeUp = add2Ready,
	Running = function () return sche.runningco end,
	GetCoByIdentity = function (identity) return sche.allcos[identity] end,
	Schedule = Schedule,
	GetCoCount = function () return  sche.co_count  end,
	Exit = Exit,
}
