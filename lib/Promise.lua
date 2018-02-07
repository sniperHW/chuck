local M = {}

local promise = {}
promise.__index = promise

local PENDING  = 1
local RESOLVED = 2
local REJECTED = 3
local resolve
local reject

function M.init(event_loop)
	if nil == M.event_loop then
		M.event_loop = event_loop
	end
	return M
end

local function isPromise(value)
	return getmetatable(value) == promise
end

local function isFunction(f)
	if type(f) == 'table' then
		local mt = getmetatable(f)
		return mt ~= nil and type(mt.__call) == 'function'
	end
	return type(f) == 'function'
end

local function fire(promise,state,value)
	if promise.state == PENDING then
		promise.value = value
		local ok = true

		if value == promise then
		   ok = false
		   promise.value = "TypeError"
		elseif isPromise(value) then
			if value.state == PENDING then
				table.insert(value.queue,promise)
				return	
			else
				state = value.state
				promise.value = value.value
			end
		else 
			local xpcall_err
			if state == REJECTED and promise.failure then
				ok,promise.value = xpcall(promise.failure,function (err) xpcall_err = err end,value)
				if not ok then
					promise.value = xpcall_err
				end
			elseif state == RESOLVED and promise.success then
				ok,promise.value = xpcall(promise.success,function (err) xpcall_err = err end,value)
				if not ok then
					promise.value = xpcall_err
				end				
			end
		end

		if not ok then
			promise.state = REJECTED
		else
			promise.state = state
		end

		if isPromise(promise.value) and promise.value.state == PENDING then
			promise.value.parent = promise
			return
		end

		if promise.parent then
			for _ , v in ipairs(promise.queue) do
				table.insert(promise.parent.queue,v)
			end
			promise.queue = promise.parent.queue
			promise.parent.queue = {}
		end
		if #promise.queue > 0 then
			local value = promise.value
			local parent = promise
			if isPromise(value) then
				parent = value
				value = parent.value
			end
			for _ , p in ipairs(promise.queue) do
				if parent.state == REJECTED and not parent.failure then
					reject(p)(value) 	
				else
					resolve(p)(value)
				end
			end
		end
	end
end

resolve = function (promise) 
	return function (value)
		M.event_loop:PostClosure(function ()
			fire(promise,RESOLVED,value)
		end)
	end
end

reject = function (promise)
	return function (err)
		M.event_loop:PostClosure(function ()
			fire(promise,REJECTED,err)
		end)
	end
end

local function newPromise(option,optionType)
	local p = {}
	p.state = PENDING
	p.queue = {}
	p = setmetatable(p, promise)
	if optionType == "function" then
		local xpcall_err
		local ok = xpcall(option,function (err) xpcall_err = err end,resolve(p), reject(p))
		if not ok then
			reject(p)(xpcall_err)
		end
	elseif optionType == "table" then
		p.success = option.success
		p.failure  = option.failure
	end
	return p
end

function M.new(func)
	if type(func) ~= 'function' then
		return nil
	end
	return newPromise(func,"function")
end

function M.all(args)
	local d = newPromise({},"table")
	if #args == 0 then
		return resolve(d)({})
	end
	local pending = #args
	local results = {}

	local function synchronizer(i, resolved)
		return function(value)
			results[i] = {
				state = resolved and "resolved" or "rejected",
				value = value
			}
			pending = pending - 1
			if pending == 0 then
				if resolved then
					resolve(d)(results)
				else
					reject(d)(results)
				end
			end
			return value
		end
	end

	for i = 1, pending do
		args[i]:andThen(synchronizer(i, true), synchronizer(i, false))
	end
	return d
end


function M.race(args)
	local d = newPromise({},"table")
	for _, v in ipairs(args) do
		v:andThen(function(res)
			resolve(d)(res)
		end, function(err)
			reject(d)(err)
		end)
	end
	return d	
end


function M.resolve(value)
	return M.new(function (resolve,_)
		resolve(value)
	end)
end

function M.reject(err)
	return M.new(function (_,reject)
		reject(err)
	end)	
end

M.throw = M.reject

--instance method

function promise:andThen(success,failure)

	if self.finally then
		error("final")
	end

	success = isFunction(success) and success or nil
	failure = isFunction(failure) and failure or nil

	local next = newPromise({success=success,failure=failure},"table")

	local p

	if isPromise(self.value) then
		p = self.value
	else
		p = self
	end
	
	if p.state == PENDING then
		table.insert(p.queue,next)
	else
		if p.state == REJECTED and not p.failure then
			reject(next)(p.value)
		else
			resolve(next)(p.value)
		end
	end
	
	return next
end

function promise:catch(failure)
	return self:andThen(nil,failure)
end

function promise:final(final)
	local next = self:andThen(final,final)
	next.finally = true
	return next
end

return M