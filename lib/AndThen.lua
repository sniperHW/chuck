--=============================================================================
-- Platform specific
--=============================================================================

local function doafter(ms, f)
	dolater(ms, f)
end

--=============================================================================

local function log(...) 
--	print('\t\t\t\t[PROMISE]', ...) 
end

--=============================================================================
-- A simple task queue that is drained on the next turn of the event loop
--=============================================================================

local Q = {}

local function drain()
	log('Drain', #Q)
	local q = Q
	Q = {}
	for _, f in ipairs(q) do
		f()
	end
end

local function queue(f, p)
	assert(f)
	table.insert(Q, function() f(p) end)
	if #Q == 1 then
		doafter(0, drain)
	end
end

--=============================================================================

local PROMISE_NEW 	   = 1
local PROMISE_RESOLVED = 2
local PROMISE_REJECTED = 3

local PROMISE = {}
PROMISE.__index = PROMISE

--=============================================================================

local function is_promise(thing)
	return getmetatable(thing) == PROMISE
end

--=============================================================================
-- Transferring a value or another promise into the one controlled by handler
--=============================================================================

local function become(thing, handler)
	if is_promise(thing) then
		local promise = thing
		if promise:is_resolved() then
			handler:resolve(promise.value)
		elseif promise:is_rejected() then
			handler:reject(promise.value)
		else
			promise:_chain(function(incoming) become(incoming, handler) end)
		end
	else
		handler:resolve(thing)
	end
end

--=============================================================================
-- Promise methods
--=============================================================================

function PROMISE:is_new()
	return self.state == PROMISE_NEW
end

function PROMISE:is_done()
	return self.state ~= PROMISE_NEW
end

function PROMISE:is_resolved()
	return self.state == PROMISE_RESOLVED
end

function PROMISE:is_rejected()
	return self.state == PROMISE_REJECTED
end

-- The then

function PROMISE:then_(resolved, rejected)
	local incoming = self
	return Promise(function(handler) 
		local function fulfill(promise)
			assert(promise:is_done())
			-- If there is a callback (either resolved or rejected passed in)
			-- we call it
			local callback
			if promise:is_resolved() then
				callback = resolved
			else
				callback = rejected
			end
			if callback then
				local success, new_value = pcall(callback, promise.value)
				if not success then
					handler:reject(new_value)
				else
					become(new_value, handler)
				end
			else
				become(promise, handler)
			end
		end
		if incoming:is_done() then
			queue(fulfill, incoming)
		else
			incoming:_chain(fulfill)
		end
	end)
end

PROMISE.and_then = PROMISE.then_

-- Executes if the incoming promise was rejected

function PROMISE:catch(rejected)
	return self:then_(nil, rejected)
end

function PROMISE:fail(rejected)
	return self:then_(nil, rejected)
end

-- Executes regardless of whether the incoming promise
-- Calls callback with a state snapshot of the incoming 
-- promise and forwards the initial promises state
-- If the callback returns a promise, it will wait until
-- that promise if fulfilled, but will always resolve to
-- the incoming value

function PROMISE:finally(callback)
	assert(callback)	
	return self:then_(function(value)
		local result = callback({resolved = true, value = value})
		if is_promise(result) then
			return result:then_resolve(value)
		end
		return value
	end,
	function(value)
		local result = callback({rejected = true, value = value})
		if is_promise(result) then
			return result:then_reject(value)
		end
		error(value, 0)
	end)
end

-- If the incoming promise is rejected, throws an error
-- in the next turn of the event loop

function PROMISE:done()
	self:then_(nil,function(value)
		queue(error, value)
	end)
end

-- Prints out a state snapshot of the incoming promise
-- and forwards its state

function PROMISE:print()
	return self:finally(function(snapshot) 
		print((snapshot.resolved and 'RESOLVED') or 'REJECTED', 
			'value =', snapshot.value)
	end)	
end
-- Resolves to the incoming value after the given delay

function PROMISE:delay(ms)
	return self:then_(function(value)
		return Promise(function(handler) 			
			doafter(ms, function()
				handler:resolve(value)
			end)
		end)
	end)
end

-- If the incoming promise does not finish within the given
-- amount of time, this will reject it with value.
-- If the incoming is already done, nothing happens.

function PROMISE:timeout(ms, value)
	local incoming = self
	if not incoming:is_done() then
		doafter(ms, function() incoming:_reject(value) end)
	end
	return incoming
end

-- Resolves immediately to the given value

function PROMISE:then_resolve(value)
	return self:then_(function()
		return value
	end)
end

-- Rejects immediately with the given value

function PROMISE:then_reject(value)
	return self:then_(function()
		error(value, 0)
	end,
	function()
		error(value, 0)
	end)
end

-- Expects an array of promises. Waits until they
-- are ALL resolved or ONE is rejected. If they are
-- all resolved, resolves to an array of their values
-- in the same order. If any one is rejected, rejects
-- with that value

function PROMISE:all()
	return self:then_(function(promises)
		return Promise(function(handler)
			local count = #promises
			local results = {}
			if count == 0 then
				handler:resolve(results)
			else
				for i, promise in ipairs(promises) do
					promise:then_(function(value)
						results[i] = value
						count = count - 1
						if count == 0 then
							handler:resolve(results)
						end
					end,
					function(value)
						handler:reject(value)
					end)
				end
			end
		end)
	end)
end

-- Expects an array of promises. Waits until they are 
-- ALL resolved OR rejected. Resolves to an array of
-- state snapshots, each one containing the value and
-- a boolean 'resolved' if it was resolved or 'rejected'
-- if it failed

function PROMISE:all_settled()
	return self:then_(function(promises)
		return Promise(function(handler)
			local count = #promises
			local results = {}
			if count == 0 then
				handler:resolve(results)
			else
				local function fulfill(i, value)
					results[i] = value
					count = count - 1
					log('Finished promise',i,'with',value,'have',count,'left')
					if count == 0 then
						handler:resolve(results)
					end
				end
				for i, promise in ipairs(promises) do
					promise:then_(function(value)
						fulfill(i, {resolved = true, value = value})
					end,
					function(value)
						fulfill(i, {rejected = true, value = value})
					end)
				end
			end
		end)
	end)
end

--=============================================================================
-- Private-ish
--=============================================================================

-- Adds a function to this promise that will be called when _invoke_chain is 
-- called (which happens automatically when a promise goes from pending to
-- resolved or rejected). You can only do this when a promise is new/pending
-- The callback receives the promise as its first and only argument

function PROMISE:_chain(callback)
	assert(callback)
	assert(self:is_new())
	local chain = self.chain
	if not chain then
		chain = {}
		self.chain = chain
	end
	table.insert(chain, callback)
end

-- Calls all the callbacks that have been added to this promise with _chain
-- You can only do this when the promise is resolved or rejected 

function PROMISE:_invoke_chain()
	assert(self:is_done())
	local chain = self.chain
	if chain then
		self.chain = nil
		for _, callback in ipairs(chain) do
			callback(self)
		end
	end
end

-- Switches state to resolved, takes on the given value and calls all the
-- callbacks in the chain. Can only call it once.

function PROMISE:_resolve(value)		
	if self:is_new() then
		assert(not is_promise(value))
		self.state = PROMISE_RESOLVED
		self.value = value
		self:_invoke_chain()
	end
end

-- Switches state to rejected, takes on the given value and calls all the
-- callbacks in the chain. Can only call it once.

function PROMISE:_reject(value)
	if self:is_new() then
		assert(not is_promise(value))
		self.state = PROMISE_REJECTED
		self.value = value
		self:_invoke_chain()
	end
end

--=============================================================================
-- Resolver
--=============================================================================

local RESOLVER = {}
RESOLVER.__index = RESOLVER;

function RESOLVER:resolve(value)
	self._promise:_resolve(value)
end

function RESOLVER:reject(value)
	self._promise:_reject(value)
end

local function Resolver(promise)
	return setmetatable({_promise = promise}, RESOLVER)
end

--=============================================================================
-- Promise constructor
--=============================================================================
-- If you pass a function, that function will be called synchronously with the
-- resolver for the new promise. If the function throws an error, the new 
-- promise will be rejected immediately.
--
-- If you pass a value that is not a promise, the new promise will be resolved
-- with that value immediately.
--
-- If you pass another promise, the new one will assume the other's value -
-- either immediately if it is resolved or rejected, or later if it is not.

local N = 1

function Promise(handler) 

	log('Created promise', N)
	N = N +1

	local promise = setmetatable({state = PROMISE_NEW}, PROMISE)
	local resolver = Resolver(promise)

	if type(handler) ~= 'function' then
		become(handler, resolver)
	else
		local success, value = pcall(handler, resolver)
		if not success then
			resolver:reject(value)
		end
	end	
	return promise
end

function PromiseThen(resolved, rejected)
	return Promise():and_then(resolved, rejected)
end
