chuck是一个lua网络开发套件.它的主要设计目标是提供开发网络程序所必须的接口,供使用者组装成适合自己的开发框架.

##特色

* 异步事件模型
* Http服务器和客户端(待完善)
* WebSocket(待实现)
* 自定义封包
* 基于coroutine的同步RPC接口
* 自定以RPC协议
* Redis访问接口(异步接口,基于coroutine的同步接口)
* 基于消息通信的多线程
* SSL(待实现)
* 支持TCP/UDP/Unix Domain Socket
* coroutine框架
* 支持Linux/FreeBSD/Mac(FreeBSD,Mac待完善)
* 支持luajit

##安装

首先编译deps目录下的lua53(如果你的系统中没有安装)

在根目录下执行`make chuck`

##Package及API

chuck的核心接口使用C语言实现,被编译成动态库,通过require语句导入.导入库分为以下Package:

* packet
* decoder
* connection
* connector
* acceptor
* socket_helper
* engine
* TimingWheel
* cthread
* error
* redis
* log
* daemon
* process

###packet

packet是chuck网路通信中的核心概念,chuck中从网络接收和发送的对象都是packet对象.目前chuck的packet分别为wpacket,rpacket,rawpacket和httppacket.

wpacket,rpacket定义了一种16/32字节包头的二进制包结构,它们需要配对使用,rpacket用于接收wpacket用于发送.

rawpacket表示原始的二进制数据,可同时用于读写

httppacket表示一个完整的http包


packet.rpacket(o) : 接受一个rpacket或wpacket对象作为参数,返回一个rpacket对象.这个对象共享底层缓冲,但拥有自己独立的读位置记录.

packet.wpacket(o) : 当o为rpacket或wpacket对象,返回一个wpacket对象.这个对象共享底层缓冲.但拥有自己独立的写位置记录.如果o为整数,则创建一个wpacket,其初始缓冲大小为o字节.如果o为nil则返回一个初始缓冲大小为64字节的wpacket.

packet.rawpacket(o) : 当o为string,返回一个rawpacket,缓冲区的内容就是string.当o为一个rawpacket则返回一个rawpacket对象,它们共享底层缓冲. 

packet.clone(o) : 返回o的一个克隆对象.


wpacket:WriteU8  : 向wpacket末尾添加一个无符号8位整数

wpacket:WriteU16 : 向wpacket末尾添加一个无符号16位整数

wpacket:WriteU32 : 向wpacket末尾添加一个无符号32位整数

wpacket:WriteNum : 向wpacket末尾添加一个double       

wpacket:WriteStr : 向wpacket末尾添加一个字符串
 
wpacket:WriteTab : 向wpacket末尾添加一个luatable(元字段,userdata等会被忽略)  



rpacket:ReadU8   : 读取一个无符号8位整数

rpacket:ReadU16  : 读取一个无符号16位整数

rpacket:ReadU32  : 读取一个无符号32位整数

rpacket:ReadI8   : 读取一个有符号8位整数

rpacket:ReadI16  : 读取一个有符号16位整数

rpacket:ReadI32  : 读取一个有符号32位整数       

rpacket:ReadNum  : 读取一个double     

rpacket:ReadStr  : 读取字符串

rpacket:ReadTab  : 读取luatable


rawpacket:ReadBin : 将底层缓冲作为一个string返回


httppacket:Method    : 返回http method 

httppacket:Status    : 返回http status

httppacket:Content   : 返回http body

httppacket:Url       : 返回http url

httppacket:Headers   : 将所有的head返回到一个lua table中 

httppacket:Header(h) : 返回head中h的value 

###decoder

decoder用于提供解包策略

decoder.connection.rpacket(max) : 返回一个最大数据大小为max的rpacket解包器

decoder.connection.http(max)    : 返回一个最大数据大小为max的httppacket解包器

decoder.connection.rawpacket    : 返回一个rawpacket解包器


decoder.datagram.rpacket(max)   : 意义与connection.rpacket一样,供数据报连接使用

decoder.datagram.rawpacket(max) : 意义与connection.rawpacket,供数据报连接使用

###connection

connection表示一个面向流的连接

connection.connection(fd,recvbuf,decoder) : 返回一个connection对象,其接收缓冲大小为recvbuf,使用decoder解包

connection:Send(packet,on_send_finish)    : 发送packet,on_send_finish如果不为空,那么当packet发送完成将会回调它

connection:Close                          : 关闭连接

connection:Add2Engine(engine,on_packet)   : 将连接关联到事件引擎,当连接上有数据事件或错误事件是回调on_packet

connection:RemoveEngine                   : 移除与事件引擎的关联(之后无法收到任何事件)

connection:SetRecvTimeout                 : 设置接收超时(如果在设定时间内连接上没有接收到任何数据,回调on_packet,errno参数为ERVTIMEOUT)

connection:SetSendTimeout                 : 设置发送超时(如果一个packet在发送队列中超过设定时间任然无法被发送,回调on_packet,errno参数为ESNTIMEOUT)

###connector

非阻塞异步连接器

connector.connector(fd,timeout)           : 返回一个连接器,如果timeout不为空那么经过timeout之后任然无法建立连接则返回错误.

connector:Add2Engine(engine,callback)     : 与事件引擎关联,连接成功或失败都会回调callback.(连接器创建之后必须与事件引擎关联,否则无法获取连接事件)

###acceptor

连接接受器

acceptor.acceptor(fd)                     : 返回一个接受器对象

acceptor:Add2Engine(engine,callback)      : 与事件引擎关联,当新连接到达或出错回调callback 

acceptor:Enable                           : 恢复接受器(恢复非屏蔽状态) 

acceptor:Disable                          : 禁止接受器(屏蔽事件)

acceptor:Close                            : 关闭监听器

###socket_helper

提供socket相关的一些接口函数

socket_helper.socket(family,type,protocol) : 创建一个套接字

socket_helper.connect(fd,ip,port)          : 为fd建立到ip:port的连接

socket_helper.listen(fd,ip,port)           : 为fd在ip:port上建立监听

socket_helper.noblock                      :

socket_helper.addr_reuse                   :

socket_helper.close                        : 关闭fd

socket_helper.gethostbyname_ipv4(host)     : 返回host的所有ip地址


###示例1

下面是网络包相关的一些示例程序:

echoserver.lua,一个简单的回射服务,可通过telnet测试


	local chuck = require("chuck")
	local socket_helper = chuck.socket_helper
	local connection = chuck.connection
	local packet = chuck.packet
	local signal = chuck.signal

	local engine

	local function sigint_handler()
		print("recv sigint")
		engine:Stop()
	end

	local signaler = signal.signaler(signal.SIGINT)


	function on_new_client(fd)
		print("on new client\n")
		local conn = connection(fd,4096)
		conn:Add2Engine(engine,function (_,p,err)
			if p then
				p = packet.clone(p)
				print(p)
				conn:Send(p,function (_)
						  	print("packet send finish")
						  	conn:Close()
							conn = nil
						  end)
			else
				conn:Close()
				conn = nil
			end
		end)
	end

	local fd =  socket_helper.socket(socket_helper.AF_INET,
									 socket_helper.SOCK_STREAM,
									 socket_helper.IPPROTO_TCP)
	socket_helper.addr_reuse(fd,1)


	local ip = "127.0.0.1"
	local port = 8010

	if 0 == socket_helper.listen(fd,ip,port) then
		print("server start",ip,port)
		local server = chuck.acceptor(fd)
		engine = chuck.engine()
		server:Add2Engine(engine,on_new_client)
		local t = chuck.RegTimer(engine,1000,function() collectgarbage("collect") end)
		signaler:Register(engine,sigint_handler)
		engine:Run()
	end

rpacket_server.lua 一个使用rpacket封包的广播回射服务,客户端发送过来的每个包都会被广播给所有的客户端

	local chuck = require("chuck")
	local socket_helper = chuck.socket_helper
	local acceptor = chuck.acceptor
	local connection = chuck.connection
	local packet = chuck.packet
	local decoder = chuck.decoder
	local signal  = chuck.signal

	local fd =  socket_helper.socket(socket_helper.AF_INET,
									 socket_helper.SOCK_STREAM,
									 socket_helper.IPPROTO_TCP)
	socket_helper.addr_reuse(fd,1)

	local engine
	local packetcout = 0

	local clients = {}

	function on_packet(conn,p,err)
		if p then
			packetcout = packetcout + 1
			conn:Send(packet.wpacket(p))
		else
		  	for k,v in pairs(clients) do
		  		if v == conn then
		  			table.remove(clients,k)
		  			break
		  		end
		  	end		
			conn:Close()
		end
	end

	function on_new_client(fd)
		print("on_new_client")
		local conn = connection(fd,65535,decoder.connection.rpacket(65535))
		conn:Add2Engine(engine,on_packet)
		table.insert(clients,conn)--hold the conn to prevent lua gc
	end

	local ip = "127.0.0.1"
	local port = 8010

	local function sigint_handler()
		print("recv sigint")
		engine:Stop()
	end

	local signaler = signal.signaler(signal.SIGINT)

	if 0 == socket_helper.listen(fd,ip,port) then
		print("server start",ip,port)
		print("server start")
		local server = acceptor(fd)
		engine = chuck.engine()
		server:Add2Engine(engine,on_new_client)
		local t = chuck.RegTimer(engine,1000,function() 
			collectgarbage("collect")
			print(packetcout)--chuck.get_bytebuffer_count())
			packetcout = 0		
		end)
		signaler:Register(engine,sigint_handler)
		engine:Run()
	end

rpacket_client.lua 

	local chuck = require("chuck")
	local socket_helper = chuck.socket_helper
	local connection = chuck.connection
	local packet = chuck.packet
	local decoder = chuck.decoder
	local errno = chuck.error
	local signal = chuck.signal


	local engine = chuck.engine()


	local function sigint_handler()
		print("recv sigint")
		engine:Stop()
	end

	local signaler = signal.signaler(signal.SIGINT)


	function connect_callback(fd,errnum)
		if errnum ~= 0 then
			socket_helper.close(fd)
		else
			print("connect success2")
			local conn = connection(fd,4096,decoder.connection.rpacket(4096))
			if conn then
				conn:Add2Engine(engine,function (_,p,err)
					print(p,err)
					if(p) then
						conn:Send(packet.wpacket(p))
					else
						conn:Close()
						conn = nil
					end
				end)

				local wpk = packet.wpacket(512)
				wpk:WriteTab({i=1,j=2,k=3,l=4,z=5})
				conn:Send(wpk)
			else
				print("create connection error")
			end		
		end	
	end

	for i = 1,100 do
		local fd =  socket_helper.socket(socket_helper.AF_INET,
										 socket_helper.SOCK_STREAM,
										 socket_helper.IPPROTO_TCP)

		socket_helper.noblock(fd,1)

		local ret = socket_helper.connect(fd,"127.0.0.1",8010)

		if ret == 0 then
			connect_callback(fd,0)
		elseif ret == -errno.EINPROGRESS then
			local connector = chuck.connector(fd,5000)
			connector:Add2Engine(engine,function(fd,errnum)
											--use closure to hold the reference of connector
											connect_callback(fd,errnum)
											connector = nil 
										end)
		else
			print("connect to 127.0.0.1 8010 error")
		end
	end

	local t = chuck.RegTimer(engine,1000,function() 
		collectgarbage("collect")
	end)

	signaler:Register(engine,sigint_handler)
	engine:Run()

	