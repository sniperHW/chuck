local chuck   =  require("chuck")
local Log     =  chuck.log
local SysLog  =  Log.SysLog
local Sche    =  require "distri.uthread.sche"
local engine  = require("distri.engine")

local current_socket    --记录当前正在处理RPC的socket,在RPC函数中可以通过RPC.Socket获取

local WakeUp    = Sche.WakeUp
local CurrentCo = Sche.Running
local Wait      = Sche.Wait

local function gen_rpc_identity()
	local g_counter = 0
	return function ()
		g_counter = math.modf(g_counter + 1,0xffffffff)
		return {h=os.time(),l=g_counter}
	end	
end

gen_rpc_identity = gen_rpc_identity()

--[[local function identity_to_string(identity)
	return string.format("%d:%d",identity.h,identity.l)
end]]--


local rpc_config = {}

function rpc_config.new(encoder,decoder,serializer,unserializer)
	if not encoder or not decoder then
		return nil
	end
	local o        = {}   
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

function rpc_server:ProcessRPC(peer,req)
	local config = self.config
	req = config.decoder(req)
	local unserializer = config.unserializer
	if unserializer then
		req = unserializer(req)
	end
	local identity   = req.id
	local rpc_record = peer.rpc_record
	
	if not (identity.l > rpc_record[2] or identity.h >  rpc_record[1]) then
		--请求重传,直接返回
		return
	end
	rpc_record[1] = identity.h
	rpc_record[2] = identity.l
	local response = {id = identity}
	local funname  = req.f
	local func 	   = self.service[funname]
	if not func then
		response.err = funname .. " not found"
	else	
		current_socket = s
		local stack,errmsg
		local ret = {xpcall(func,function (err)
						errmsg = err
						stack  = debug.traceback()
					end,table.unpack(req.arg))}
		current_socket = nil
		if ret[1] then
			table.remove(ret,1)			
			response.ret = ret
		else
			response.err = errmsg
			SysLog(Log.ERROR,string.format("error on [task:Do]:%s\n%s",errmsg,stack))
		end
	end
	peer:Send(config.encoder(response))
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
	local co = CurrentCo()
	if not co then
		return "rpc_client:Call should call under coroutine"
	end
	local peer = self.peer
	if not peer then
		return "rpc_client not connect to peer"
	end
	local config = self.config
	local request = {}
	request.f = func
	request.id = gen_rpc_identity()
	--local id_string = identity_to_string(request.id)
	request.arg = {...}
	local serializer = config.serializer
	if serializer then
		request = serializer(request)
	end

	if not peer:Send(config.encoder(request)) then
		return "socket error"
	end
	local idx = request.id.l
	peer.pending_call[idx] = co

	local c = 3
	co.timer = chuck.RegTimer(engine,1000,function ()
		c = c - 1
		if c == 0 then
			WakeUp(context.co,"rpc timeout")
			co.timer = nil
			return -1
		end
		peer:Send(config.encoder(request))
		if c == 2 then
			return 2000
		else
			return 5000
		end
	end)
	local err,response = Wait()
	if co.timer then
		chuck.RemTimer(co.timer)
		co.timer = nil
	end		
	peer.pending_call[idx] = nil
	if not response then
		return err
	elseif response.err then
		return response.err 
	else
		return nil,table.unpack(response.ret)
	end
end


local function OnRPCResponse(config,peer,res)
	res = config.decoder(res)
	if config.unserializer then
		res = config.unserializer(res)
	end
	local co = peer.pending_call[res.id.l]
	if co then	
		WakeUp(co,"ok",res)
	else
		SysLog(Log.ERROR,string.format("unknow rpc response %d%d",res.id.h,res.id.l))
	end
end

return {
	OnRPCResponse = OnRPCResponse,
	Config        = rpc_config.new,
	Server        = function(config) return rpc_server:new(config) end,
	Client        = function(config) return rpc_client:new(config) end,
	Socket        = current_socket
}