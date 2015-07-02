local chuck   =  require("chuck")
local Log     =  chuck.log
local SysLog  =  Log.SysLog
local Sche    = require "distri.uthread.sche"

local function gen_rpc_identity()
	local g_counter = 0
	return function ()
		g_counter = math.modf(g_counter + 1,0xffffffff)
		return {h=os.time(),l=g_counter} 
	end	
end

gen_rpc_identity = gen_rpc_identity()

local function identity_to_string(identity)
	return string.format("%d:%d",identity.h,identity.l)
end


local rpc_config = {}

function rpc_config:new(encoder,decoder,serializer,unserializer)
	if not encoder or not decoder then
		return nil
	end
	local o        = {}   
	o.__index      = rpc_config
	setmetatable(o, o)
	o.decoder      = decoder        --将数据从packet中取出
	o.encoder      = encoder        --使用特定封包格式将数据封装到packet中      
	o.serializer   = serializer     --将lua table序列化成传输格式,可以为空
	o.unserializer = unserializer   --将传输格式的数据反序列化成lua table,可以为空
	return o
end


local rpc_server = {}

function rpc_server:new(rpcconfig)
	if not rpcconfig then
		return nil
	end
	local o        = {}   
	o.__index      = rpc_server
	setmetatable(o, o)
	o.config 	   = rpcconfig
	o.service      = {}
	return o
end

--注册一个名为name的rpc服务
function rpc_server:RegService(name,func)
	self.service[name] = func
end

function rpc_server:ProcessCall(peer,req)
	req = self.config.decoder(req)
	local unserializer = self.config.unserializer
	if unserializer then
		req = unserializer(req)
	end

	local identity = req.identity
	local response = {identity = identity}
	local funname  = req.f
	local func 	   = self.service[funname]
	if not func then
		response.err = funname .. " not found"
	else	
		local stack,errmsg
		local ret = table.pack(xpcall(func,
									  function (err)
									  	errmsg = err
									  	stack  = debug.traceback()
									  end,s,table.unpack(req.arg)))
		if ret[1] then
			table.remove(ret,1)			
			response.ret = ret
		else
			response.err = errmsg
			SysLog(Log.ERROR,string.format("error on [task:Do]:%s\n%s",errmsg,stack))
		end
	end
	peer:Send(self.config.encoder(response))
end


local rpc_client = {}

function rpc_client:new(rpcconfig)
	if not rpcconfig then
		return nil
	end
	local o        = {}   
	o.__index      = rpc_client
	setmetatable(o, o)
	o.config 	   = rpcconfig
	o.service      = {}
	return o
end

function rpc_client:Connect(peer)
	if self.peer then
		return
	end
	self.peer = peer
	return true
end

function rpc_client:Call(func,...)
	local co = Sche.Running()
	if not co then
		return "rpc_client:Call should call under coroutine"
	end
	local peer = self.peer
	if not peer then
		return "rpc_client not connect to peer"
	end

	local request = {}
	request.f = func
	request.identity = gen_rpc_identity()
	local id_string = identity_to_string(request.identity)
	request.arg = {...}
	local serializer = self.config.serializer
	if serializer then
		request = serializer(request)
	end

	if not peer:Send(self.config.encoder(request)) then
		return "socket error"
	end
	local context = {co = co}
	peer.pending_call = peer.pending_call or {}
	peer.pending_call[id_string]  = context	
	local response = Sche.Wait()
	if not response.ret then
		return response.err,nil
	else
		return response.err,table.unpack(response.ret)
	end
end


local function OnRpcResponse(config,peer,res)
	res = config.decoder(res)
	if config.unserializer then
		res = config.unserializer(res)
	end

	local id_string = identity_to_string(res.identity)
	local context = peer.pending_call[id_string]
	if context then
		peer.pending_call[id_string] = nil	
		Sche.WakeUp(context.co,res)
	else
		SysLog(Log.ERROR,string.format("unknow rpc response %s",id_string))
	end
end


return {
	OnRpcResponse = OnRpcResponse,
	Config     = function(...)    return rpc_config:new(...) end,
	Server     = function(config) return rpc_server:new(config) end,
	Client     = function(config) return rpc_client:new(config) end,
}