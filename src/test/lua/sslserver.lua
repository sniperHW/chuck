package.path = './lib/?.lua;'
package.cpath = './lib/?.so;'

local chuck = require("chuck")
local socket = chuck.socket
local packet = chuck.packet
local ssl = chuck.ssl

local event_loop = chuck.event_loop.New()


local ssl_ctx

local function init_ssl()
	local _ssl_ctx = ssl.SSL_CTX_new("SSLv23_server_method")

	if not _ssl_ctx then
		return nil 
	end

	local err = ssl.SSL_CTX_use_certificate_file(_ssl_ctx,"cacert.pem")

	if err then
		print("SSL_CTX_use_certificate_file failed:" .. err)
		return nil
	end

	err = ssl.SSL_CTX_use_PrivateKey_file(_ssl_ctx,"privkey.pem")

	if err then
		print("SSL_CTX_use_PrivateKey_file failed:" .. err)		
		return nil
	end

	err = ssl.SSL_CTX_check_private_key(_ssl_ctx)

	if err then
		print("SSL_CTX_check_private_key failed:" .. err)				
		return nil
	end

	return _ssl_ctx

end


local ssl_ctx = init_ssl()

if ssl_ctx then 
	local server = socket.stream.ip4.listen(event_loop,"127.0.0.1",8010,function (fd,err)

		if not fd then
			print("err:" .. err)
			return
		end

		local conn = socket.stream.New(fd,4096,packet.Decoder(65536))
		if conn then
			local err = ssl.SSL_accept(conn,ssl_ctx)
			if err then
				print("SSL_accept error:" .. err)
				conn:Close()
				return
			end

			conn:Start(event_loop,function (data)
				if data then
					conn:Close()
					--conn:Send(data)
				else
					print("client disconnected") 
					conn:Close()
				end
			end)
		end
	end)

	print("server run")

	if server then
		event_loop:WatchSignal(chuck.signal.SIGINT,function()
			print("recv SIGINT stop server")
			event_loop:Stop()
		end)
		event_loop:Run()
	end

else
	print("ssl init error")
end