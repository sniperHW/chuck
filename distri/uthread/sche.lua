local LinkQue =  require "distri.linkque"
local chuck   =  require("chuck")
local Log     =  chuck.log
local SysLog  =  Log.SysLog 

--for luajit
if not table.unpack then
	table.unpack = unpack
end

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
local stat_wait    = 5
local stat_running = 6

local function add2Ready(co,...)
    local status = co.status
    if status == stat_ready or status == stat_dead or status == stat_running then
    	return
    end
    co.status = stat_ready
    if co.wheel then
    	co.wheel:UnRegister()
    	co.wheel = nil
    end    
    co.__wait_ret = {...}
    sche.ready:Push({co}) 
end


local function yield()
	local co = sche.runningco
	if co.status ~= stat_running then
		return
	end
	co.status = stat_yield
	coroutine.yield(co.coroutine)	
end

local function _wait(co,ms)
    if ms then
	    co.wheel = sche.timer:Register(ms,function ()
	       	add2Ready(co,"timeout")
	    end)
    end
	co.status = stat_wait
	coroutine.yield(co.coroutine)

	return table.unpack(co.__wait_ret)
	 	
end

local function wait(ms)
	local co = sche.runningco
	if co.status ~= stat_running then
		return
	elseif ms == 0 then
		return yield()
	end	
	if co.wait_func then
		return co.wait_func(co,ms)
	else
		return _wait(co,ms)
	end
end

local function _sleep(co,ms)
	co.wheel = sche.timer:Register(ms,function ()
	    add2Ready(co)
	end)
	co.status = stat_sleep
	coroutine.yield(co.coroutine)   	
end

local function sleep(ms)
	local co = sche.runningco
	if co.status ~= stat_running then
		return
	elseif not ms or ms == 0 then
		return yield()
	end	
	if co.sleep_func then
		co.sleep_func(co,ms)
	else
		_sleep(co,ms)
	end
end

local function switchTo(co)
	local pre_co = sche.runningco
	sche.runningco = co
	co.status = stat_running
	coroutine.resume(co.coroutine,co)
	sche.runningco = pre_co
	return co.status	
end

local function Schedule(co,...)
	local readylist = sche.ready
	if co then
		local status = co.status
		if status == stat_ready or status == stat_dead or status == stat_running then
			return sche.ready:Len()
		end
		if co.wheel then	
	    	co.wheel:UnRegister()
	    	co.wheel = nil
	    end
	    co.__wait_ret = {...}					
		if switchTo(co) == stat_yield then
			add2Ready(co)
		end		
	else
		sche.timer:Tick()
		local yields = {}
		co = readylist:Pop()
		while co do
			co = co[1]
			if switchTo(co) == stat_yield then
				table.insert(yields,co)
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
	g_counter = g_counter + 1
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

local pool = {
	init    = 100,
	max     = 65535,
	free    = LinkQue.New(),
	taskque = LinkQue.New(),
	total   = 0,
	block   = 0,
}

local ut_main

local function pool_sleep(co,ms)
	pool.block = pool.block + 1	
	if ms > 0 and pool.taskque:Len() > 0 then
		local node = pool.free:Front()
		if node then
			--当前uthread将进入睡眠,如果还有任务尝试唤醒另一空闲uthread执行task
			add2Ready(node[1])
		elseif pool.block == pool.total and pool.total < pool.max then
			--所有uthread都被阻塞,但没有超过上限,创建一个
			pool.total = pool.total + 1
			Spawn(ut_main)
		end
	end
	_sleep(co,ms)
	pool.block = pool.block - 1
end

local function pool_wait(co,ms)
	pool.block = pool.block + 1
	if pool.taskque:Len() > 0 then
		local node = pool.free:Front()
		if node then
			--当前uthread将进入等待,如果还有任务尝试唤醒另一空闲uthread执行task
			add2Ready(node[1])
		elseif pool.block == pool.total and pool.total < pool.max then
			--所有uthread都被阻塞,但没有超过上限,创建一个
			pool.total = pool.total + 1
			Spawn(ut_main)
		end
	end
	local ret = {_wait(co,ms)}
	pool.block = pool.block - 1
	return table.unpack(ret)
end

local function GetTask()
	local task
	local ut = sche.runningco
	local node = {ut}
	while true do
		task = pool.taskque:Pop()
		if task then
			return task
		end
		pool.free:Push(node)
		_wait(ut)
		pool.free:Remove(node)
	end
end

local function Do(task)
	local stack,errmsg
	if not xpcall(task.func,
		          function (err)
					errmsg = err
					stack  = debug.traceback()
		       	  end,table.unpack(task.arg))
	then
    	SysLog(Log.ERROR,string.format("error on [task:Do]:%s\n%s",errmsg,stack))
	end
end

ut_main = function ()
	local co = sche.runningco
	co.wait_func  = pool_wait
	co.sleep_func = pool_sleep
	while true do
		Do(GetTask())
		if pool.free:Len() >= pool.init then
			break
		end
	end
	pool.total = pool.total - 1
end

function pool.AddTask(task)
	if pool.total == 0 then
		for i = 1,pool.init do
			SpawnAndRun(ut_main)
		end
		pool.total = pool.init
	end	
	pool.taskque:Push(task)
	if pool.free:Len() == pool.total then
		--所有uthread都在等待任务,唤醒一个
		add2Ready(pool.free:Front()[1])
	elseif pool.block == pool.total and pool.total < pool.max then
		--所有uthread都被阻塞,但没有超过上限,创建一个
		SpawnAndRun(ut_main)
	end
end

return {
	Spawn = Spawn,
	SpawnAndRun = SpawnAndRun,
	Yield = yield,
	Sleep = sleep,
	Wait  = wait,
	WakeUp = add2Ready,
	Running = function () return sche.runningco end,
	GetCoByIdentity = function (identity) return sche.allcos[identity] end,
	Schedule = Schedule,
	GetCoCount = function () return  sche.co_count  end,
	Pool = pool,
}
