
#include "socket/chk_stream_socket_define.h"
#include "socket/chk_datagram_socket_define.h"
#include "socket/chk_acceptor_define.h"

#define ACCEPTOR_METATABLE "lua_acceptor"

#define STREAM_SOCKET_METATABLE "lua_stream_socket"

#define DGRAM_SOCKET_METATABLE "lua_datagram_socket"

#define SOCK_ADDR_METATABLE "lua_sock_addr"

#define SSL_CTX_METATABLE "lua_ssl_ctx"

typedef struct {
	chk_acceptor *c_acceptor;
}lua_acceptor;

typedef struct {
	chk_stream_socket *socket;
	chk_luaRef cb;
}lua_stream_socket;

typedef struct {
	chk_datagram_socket *socket;
	chk_luaRef cb;	
}lua_datagram_socket;

typedef struct {
	SSL_CTX *ctx;
}lua_SSL_CTX;

#define lua_checkacceptor(L,I)	\
	(lua_acceptor*)luaL_checkudata(L,I,ACCEPTOR_METATABLE)

#define lua_checkstreamsocket(L,I)	\
	(lua_stream_socket*)luaL_checkudata(L,I,STREAM_SOCKET_METATABLE)

#define lua_checkdatagramsocket(L,I)	\
	(lua_datagram_socket*)luaL_checkudata(L,I,DGRAM_SOCKET_METATABLE)	

#define lua_check_ssl_ctx(L,I)	\
	(lua_SSL_CTX*)luaL_checkudata(L,I,SSL_CTX_METATABLE)

#define lua_check_sockaddr(L,I) \
	(chk_sockaddr*)luaL_checkudata(L,I,SOCK_ADDR_METATABLE)


static void lua_acceptor_cb(chk_acceptor *_,int32_t fd,chk_sockaddr *addr,chk_ud ud,int32_t err) {
	chk_luaRef   cb = ud.v.lr;
	const char   *error;
	if(0 == err) {
		error = chk_Lua_PCallRef(cb,"i",fd);
	}else {
		error = chk_Lua_PCallRef(cb,"ps",NULL,chk_get_errno_str(err));
	}
	if(error) {
		close(fd);
		CHK_SYSLOG(LOG_ERROR,"error on lua_acceptor_cb: %s",error);		
	}
}

static int32_t lua_acceptor_gc(lua_State *L) {
	lua_acceptor *a = lua_checkacceptor(L,1);
	if(a->c_acceptor){
		chk_ud ud = chk_acceptor_get_ud(a->c_acceptor);
		chk_luaRef_release(&ud.v.lr);
		chk_acceptor_del(a->c_acceptor);
		a->c_acceptor = NULL;
	}
	return 0;
}

static int32_t lua_acceptor_pause(lua_State *L) {
	lua_acceptor *a = lua_checkacceptor(L,1);
	if(a->c_acceptor){
		chk_acceptor_pause(a->c_acceptor);
	}
	return 0;
}

static int32_t lua_acceptor_resume(lua_State *L) {
	lua_acceptor *a = lua_checkacceptor(L,1);
	if(a->c_acceptor){
		chk_acceptor_resume(a->c_acceptor);
	}
	return 0;
}

static int32_t lua_listen_ssl(lua_State *L) {
	chk_event_loop *event_loop;
	lua_acceptor   *a;
	chk_acceptor   *acceptor;
	chk_sockaddr   *addr;
	lua_SSL_CTX    *ssl_ctx;
	chk_luaRef      accept_cb;	

	event_loop = lua_checkeventloop(L,1);
	ssl_ctx    = lua_check_ssl_ctx(L,2);

	if(!ssl_ctx->ctx) {
		return luaL_error(L,"invaild ssl_ctx");
	}

	addr       = lua_check_sockaddr(L,3);

	if(!lua_isfunction(L,4)) 
		return luaL_error(L,"argument 4 of listen_ssl must be lua function");

	accept_cb = chk_toluaRef(L,4);

	acceptor = chk_ssl_listen(event_loop,addr,ssl_ctx->ctx,lua_acceptor_cb,chk_ud_make_lr(accept_cb));

	ssl_ctx->ctx = NULL;

	if(!acceptor) {
		chk_luaRef_release(&accept_cb);
		CHK_SYSLOG(LOG_ERROR,"chk_ssl_listen() failed");
		return 0;
	}

	a = LUA_NEWUSERDATA(L,lua_acceptor);
	if(!a){
		chk_acceptor_del(acceptor);	
		chk_luaRef_release(&accept_cb);
		CHK_SYSLOG(LOG_ERROR,"LUA_NEWUSERDATA() failed");
		return 0;
	}

	a->c_acceptor = acceptor;

	luaL_getmetatable(L, ACCEPTOR_METATABLE);
	lua_setmetatable(L, -2);
	return 1;	
}


static int32_t lua_listen(lua_State *L) {
	chk_event_loop *event_loop;
	lua_acceptor   *a;
	chk_acceptor   *acceptor;
	chk_luaRef      accept_cb;
	chk_sockaddr   *addr;

	event_loop = lua_checkeventloop(L,1);
	addr       = lua_check_sockaddr(L,2);
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of listen must be lua function");
	
	accept_cb = chk_toluaRef(L,3);
	acceptor  = chk_listen(event_loop,addr,lua_acceptor_cb,chk_ud_make_lr(accept_cb));

	if(!acceptor) {
		chk_luaRef_release(&accept_cb);
		CHK_SYSLOG(LOG_ERROR,"chk_listen() failed");
		return 0;
	}

	a = LUA_NEWUSERDATA(L,lua_acceptor);
	if(!a){
		chk_acceptor_del(acceptor);	
		chk_luaRef_release(&accept_cb);
		CHK_SYSLOG(LOG_ERROR,"LUA_NEWUSERDATA() failed");
		return 0;
	}

	a->c_acceptor = acceptor;

	luaL_getmetatable(L, ACCEPTOR_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static void dail_cb(int32_t fd,chk_ud ud,int32_t err) {
	chk_luaRef cb = ud.v.lr;
	const char *error;
	if(0 == err) {
		error = chk_Lua_PCallRef(cb,"i",fd);
	}else {
		error = chk_Lua_PCallRef(cb,"ps",NULL,chk_get_errno_str(err));		
	} 
	if(error) {
		close(fd);
		CHK_SYSLOG(LOG_ERROR,"error on dail_cb %s",error);		
	}
	chk_luaRef_release(&cb);
}

static int32_t lua_dail(lua_State *L) {
	chk_luaRef      cb = {0};
	chk_sockaddr   *remote;
	uint32_t        timeout = 0;
	int32_t         ret;
	chk_event_loop *event_loop;

	event_loop = lua_checkeventloop(L,1);
	remote     = lua_check_sockaddr(L,2);

	int cbIdx = -1;
	int type4 = lua_type(L,3);
	if(type4 == LUA_TNUMBER){
		timeout = (uint32_t)luaL_checkinteger(L,3);
		cbIdx = 4;
	} else {
		cbIdx = 3;
	}

	if(LUA_TFUNCTION != lua_type(L,cbIdx)) {
		lua_pushstring(L,"lua_dail missing callback function");
		return 1;		
	}

	cb = chk_toluaRef(L,cbIdx);
	ret = chk_easy_async_connect(event_loop,remote,NULL,dail_cb,chk_ud_make_lr(cb),timeout);
	if(ret != 0) {
		chk_luaRef_release(&cb);
		lua_pushstring(L,"connect error");
		return 1;
	}

	return 0;
}

static int32_t lua_stream_socket_gc(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(s->socket){
		chk_stream_socket_setUd(s->socket,chk_ud_make_void(NULL));
		chk_stream_socket_close(s->socket,0);
	}
	if(s->cb.L) {
		chk_luaRef_release(&s->cb);
	}
	return 0;
}

static void data_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	lua_stream_socket *lua_socket = (lua_stream_socket*)chk_stream_socket_getUd(s).v.val;
	if(!lua_socket) {
		return;
	}

	if(!lua_socket->cb.L) {
		return;
	}

	luaBufferPusher pusher = {PushBuffer,data};
	const char *error_str;
	if(data) {
		error_str = chk_Lua_PCallRef(lua_socket->cb,"f",(chk_luaPushFunctor*)&pusher);
	} else {
		error_str = chk_Lua_PCallRef(lua_socket->cb,"ps",NULL,chk_get_errno_str(error));
	}

	if(error_str) { 
		CHK_SYSLOG(LOG_ERROR,"error on data_cb %s",error_str);
	}	
}

static int32_t lua_stream_socket_start(lua_State *L) {
	lua_stream_socket *s;
	chk_event_loop    *event_loop; 
	s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return luaL_error(L,"invaild lua_stream_socket");
	}
	event_loop = lua_checkeventloop(L,2);
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of stream_socket_bind must be lua function");
	if(0 != chk_loop_add_handle(event_loop,(chk_handle*)s->socket,data_cb)) {
		CHK_SYSLOG(LOG_ERROR,"chk_loop_add_handle() failed");
		lua_pushstring(L,"stream_socket_bind failed");
		return 1;
	} else {
		s->cb = chk_toluaRef(L,3);
	}
	return 0;
}

static int32_t lua_stream_socket_new(lua_State *L) {
	int32_t fd;
	lua_stream_socket *s;
	chk_stream_socket *c_stream_socket;
	chk_stream_socket_option option = {
		.decoder = NULL
	};
	fd = (int32_t)luaL_checkinteger(L,1);
	option.recv_buffer_size = (uint32_t)luaL_optinteger(L,2,4096);
	if(lua_islightuserdata(L,3)) option.decoder = lua_touserdata(L,3);
	s = LUA_NEWUSERDATA(L,lua_stream_socket);
	if(!s) {
		CHK_SYSLOG(LOG_ERROR,"LUA_NEWUSERDATA(lua_stream_socket) failed");
		return 0;
	}

	memset(s,0,sizeof(*s));

	c_stream_socket = chk_stream_socket_new(fd,&option);

	if(!c_stream_socket){
		if(option.decoder) {
			option.decoder->release(option.decoder);
		}
		CHK_SYSLOG(LOG_ERROR,"chk_stream_socket_new() failed");
		return 0;
	}

	s->socket = c_stream_socket;
	chk_stream_socket_setUd(s->socket,chk_ud_make_void(s));
	luaL_getmetatable(L, STREAM_SOCKET_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}	

static int32_t lua_stream_socket_close(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return 0;
	}
	if(s->cb.L) {
		chk_luaRef_release(&s->cb);
	}	
	chk_stream_socket_setUd(s->socket,chk_ud_make_void(NULL));
	uint32_t delay = (uint32_t)luaL_optinteger(L,2,0);			
	chk_stream_socket_close(s->socket,delay);	
	s->socket = NULL;
	return 0;
}

static int32_t lua_stream_socket_pause_read(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return luaL_error(L,"invaild lua_stream_socket");
	}
	chk_stream_socket_pause_read(s->socket);
	return 0;
}

static int32_t lua_stream_socket_resume_read(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return luaL_error(L,"invaild lua_stream_socket");
	}
	chk_stream_socket_resume_read(s->socket);
	return 0;
}

static int32_t lua_close_fd(lua_State *L) {
	int32_t fd = (int32_t)luaL_checkinteger(L,1);
	close(fd);
	return 0;
}


static int32_t lua_stream_socket_send(lua_State *L) {
	chk_bytebuffer    *b,*o;
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		lua_pushstring(L,"socket close");		
		return 1;
	}
	o = lua_checkbytebuffer(L,2);
	if(NULL == o)
		luaL_error(L,"need bytebuffer to send");
	b = chk_bytebuffer_clone(o);
	if(!b) {
		lua_pushstring(L,"send error");		
		return 1;
	}

	if(0 != chk_stream_socket_send(s->socket,b)){
		lua_pushstring(L,"send error");
		return 1;
	}
	return 0;
}

static int32_t lua_stream_socket_send_urgent(lua_State *L) {
	chk_bytebuffer    *b,*o;
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		lua_pushstring(L,"socket close");		
		return 1;
	}

	o = lua_checkbytebuffer(L,2);
	if(NULL == o)
		luaL_error(L,"need bytebuffer to send");
	b = chk_bytebuffer_clone(o);
	if(!b) {
		lua_pushstring(L,"send error");		
		return 1;
	}

	if(0 != chk_stream_socket_send_urgent(s->socket,b)){
		lua_pushstring(L,"send error");				
		return 1;
	}
	return 0;
}


static int32_t lua_stream_socket_getsockaddr(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return luaL_error(L,"invaild lua_stream_socket");
	}
	chk_sockaddr *addr = LUA_NEWUSERDATA(L,chk_sockaddr);
	if(NULL == addr) {
		return 0;
	}

	if(0 != chk_stream_socket_getsockaddr(s->socket,addr)){
		return 0;
	}

	luaL_getmetatable(L, SOCK_ADDR_METATABLE);
	lua_setmetatable(L, -2);

	return 1;	
}

static int32_t lua_stream_socket_getpeeraddr(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return luaL_error(L,"invaild lua_stream_socket");
	}
	chk_sockaddr *addr = LUA_NEWUSERDATA(L,chk_sockaddr);
	if(NULL == addr) {
		return 0;
	}

	if(0 != chk_stream_socket_getpeeraddr(s->socket,addr)){
		return 0;
	}

	luaL_getmetatable(L, SOCK_ADDR_METATABLE);
	lua_setmetatable(L, -2);

	return 1;	
}

static int32_t lua_stream_socket_set_nodelay(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return 0;
	}
	int8_t on = (int8_t)luaL_optinteger(L,2,0);
	chk_stream_socket_nodelay(s->socket,on);
	return 0;
}

static int32_t lua_stream_socket_shutdown_write(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return 0;
	}
	chk_stream_socket_shutdown_write(s->socket);
	return 0;
}

static int32_t lua_inet_ntop(lua_State *L) {
	chk_sockaddr *addr = lua_check_sockaddr(L,1);
	if(NULL == addr) {
		return luaL_error(L,"invaild chk_sockaddr");
	}
	char buff[46];
	if(0 != easy_sockaddr_inet_ntop(addr,buff,sizeof(buff))) {
		return 0;
	}
	lua_pushstring(L,buff);
	return 1;
}

static void lua_socket_close_callback(chk_stream_socket *_,chk_ud ud) {
	chk_luaRef cb = ud.v.lr;
	const char *error_str;
	if(!cb.L) return;
	error_str = chk_Lua_PCallRef(cb,"");
	if(error_str) CHK_SYSLOG(LOG_ERROR,"error on lua_socket_close_callback %s",error_str);
	chk_luaRef_release(&cb);
}

static int32_t lua_stream_socket_set_close_cb(lua_State *L) {
	chk_luaRef cb  = {0};
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return 0;
	}
	if(!lua_isfunction(L,2)) 
		return luaL_error(L,"argument 2 of SetCloseCallBack must be lua function");

	cb = chk_toluaRef(L,2);
	chk_stream_socket_set_close_callback(s->socket,lua_socket_close_callback,chk_ud_make_lr(cb));
	return 0;
}

static int32_t lua_inet_port(lua_State *L) {
	chk_sockaddr *addr = lua_check_sockaddr(L,1);
	if(NULL == addr) {
		return luaL_error(L,"invaild chk_sockaddr");
	}
	uint16_t port;
	if(0 != easy_sockaddr_port(addr,&port)) {
		return 0;
	}
	lua_pushinteger(L,port);
	return 1;
}

//datagram socket

typedef struct {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
	chk_sockaddr *addr;
}luaAddrPusher;

static void PushAddr(chk_luaPushFunctor *_,lua_State *L) {
	luaAddrPusher *self = (luaAddrPusher*)_;
	chk_sockaddr  *addr = LUA_NEWUSERDATA(L,chk_sockaddr);
	if(addr) {
		*addr = *self->addr;
		luaL_getmetatable(L, SOCK_ADDR_METATABLE);
		lua_setmetatable(L, -2);		
	} else {
		CHK_SYSLOG(LOG_ERROR,"newuserdata() chk_bytebuffer failed");	
		lua_pushnil(L);
	}
}

static void datagram_event_cb(chk_datagram_socket *datagram_socket,chk_datagram_event *event,int32_t error) {

	lua_datagram_socket *s = (lua_datagram_socket*)chk_datagram_socket_getUd(datagram_socket).v.val;
	if(!s) {
		return;
	}

	if(!s->cb.L) {
		return;
	}

	const char *error_str;
	if(event) {
		luaBufferPusher pusher1 = {PushBuffer,event->buff};
		luaAddrPusher   pusher2 = {PushAddr,&event->src};		
		error_str = chk_Lua_PCallRef(s->cb,"ffi",(chk_luaPushFunctor*)&pusher1,(chk_luaPushFunctor*)&pusher2,error);
	} else {
		error_str = chk_Lua_PCallRef(s->cb,"ps",NULL,chk_get_errno_str(error));
	}

	if(error_str) { 
		CHK_SYSLOG(LOG_ERROR,"error on datagram_event_cb %s",error_str);
	}

}

static int32_t lua_datagram_socket_sendto(lua_State *L) {
	chk_bytebuffer    *b,*o;
	chk_sockaddr      *dst;
	lua_datagram_socket *s = lua_checkdatagramsocket(L,1);
	if(!s->socket){
		lua_pushstring(L,"socket close");		
		return 1;
	}
	o   = lua_checkbytebuffer(L,2);
	dst = lua_check_sockaddr(L,3);
	b   = chk_bytebuffer_clone(o);
	if(!b) {
		lua_pushstring(L,"invaild buff");		
		return 1;
	}
	if(0 != chk_datagram_socket_sendto(s->socket,b,dst)){
		lua_pushstring(L,"send error");
		return 1;
	}
	return 0;
}

static int32_t lua_datagram_socket_broadcast(lua_State *L) {
	chk_bytebuffer    *b,*o;
	chk_sockaddr      *addr;
	lua_datagram_socket *s = lua_checkdatagramsocket(L,1);
	if(!s->socket){
		lua_pushstring(L,"socket close");		
		return 1;
	}
	o    = lua_checkbytebuffer(L,2);
	addr = lua_check_sockaddr(L,3);
	b    = chk_bytebuffer_clone(o);
	if(!b) {
		lua_pushstring(L,"invaild buff");		
		return 1;
	}
	if(0 != chk_datagram_socket_broadcast(s->socket,b,addr)){
		lua_pushstring(L,"broadcast error");
		return 1;
	}
	return 0;	
}

static int32_t lua_datagram_socket_set_broadcast(lua_State *L) {
	lua_datagram_socket *s = lua_checkdatagramsocket(L,1);
	if(!s->socket){
		lua_pushstring(L,"socket close");		
		return 1;
	}
	if(chk_error_ok != chk_datagram_socket_set_broadcast(s->socket)) {
		lua_pushstring(L,"set broadcast failed");
		return 1;
	}else {
		return 0;
	}	
}

static int32_t lua_datagram_socket_close(lua_State *L) {
	lua_datagram_socket *s = lua_checkdatagramsocket(L,1);
	if(s->socket){
		chk_luaRef_release(&s->cb);		
		chk_datagram_socket_close(s->socket);
		s->socket = NULL;
	}
	return 0;
}

static int32_t lua_datagram_socket_start(lua_State *L) {
	lua_datagram_socket *s;
	chk_event_loop      *event_loop; 
	s = lua_checkdatagramsocket(L,1);
	if(!s->socket){
		return luaL_error(L,"invaild lua_datagram_socket");
	}
	event_loop = lua_checkeventloop(L,2);
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of datagram_socket_bind must be lua function");
	if(0 != chk_loop_add_handle(event_loop,(chk_handle*)s->socket,datagram_event_cb)) {
		CHK_SYSLOG(LOG_ERROR,"chk_loop_add_handle() failed");
		lua_pushstring(L,"datagram_socket_bind failed");
		return 1;
	} else {
		s->cb = chk_toluaRef(L,3);
	}	
	return 0;
}

static int32_t lua_datagram_socket_gc(lua_State *L) {
	lua_datagram_socket *s = lua_checkdatagramsocket(L,1);
	if(s->socket){
		chk_datagram_socket_setUd(s->socket,chk_ud_make_void(NULL));
		chk_datagram_socket_close(s->socket);
	}
	if(s->cb.L) {
		chk_luaRef_release(&s->cb);
	}
	return 0;	
}

static int32_t lua_datagram_socket_bind(lua_State *L) {
	lua_datagram_socket *s = lua_checkdatagramsocket(L,1);
	if(!s->socket) {
		return luaL_error(L,"socket closed");
	}
	chk_sockaddr *addr     = lua_check_sockaddr(L,2);
	if(s->socket->addr_type != addr->addr_type) {
		return luaL_error(L,"invaild addr type");
	}
    if(0 != easy_bind(s->socket->fd,addr)){
    	return luaL_error(L,"bind failed\n");
    }
    return 0;	
}

static int32_t lua_datagram_socket_new(lua_State *L) {
	chk_datagram_socket *udpsocket;
	int                  fd;
	int family = luaL_checkinteger(L,1);;
	if(!((family == AF_INET) || (family == AF_LOCAL))) {
		return luaL_error(L,"unsupport addr type");
	}
	fd = socket(family,SOCK_DGRAM,IPPROTO_UDP);
	if(fd < 0){
		return luaL_error(L,"new socket failed");
	}

	if(family == AF_INET) {
		udpsocket = chk_datagram_socket_new(fd,SOCK_ADDR_IPV4);
	} else {
		udpsocket = chk_datagram_socket_new(fd,SOCK_ADDR_UN);		
	}

    if(NULL == udpsocket){
    	close(fd);
    	return luaL_error(L,"chk_datagram_socket_new failed\n");
    }
    lua_datagram_socket *s = LUA_NEWUSERDATA(L,lua_datagram_socket);
    if(NULL == s) {
    	chk_datagram_socket_close(udpsocket);
    	return luaL_error(L,"LUA_NEWUSERDATA failed\n");
    } else {
    	s->socket = udpsocket;
    	chk_datagram_socket_setUd(s->socket,chk_ud_make_void(s));
		luaL_getmetatable(L, DGRAM_SOCKET_METATABLE);
		lua_setmetatable(L, -2);    	
    	return 1;
    }
	return 0;
}

static int32_t lua_addr(lua_State *L) {
	int family = luaL_checkinteger(L,1);
	chk_sockaddr *addr = LUA_NEWUSERDATA(L,chk_sockaddr);
	if(NULL == addr) {
		return 0;
	}	
	if(family == AF_INET) {
		const char *ip = luaL_checkstring(L,2);
		int port = luaL_checkinteger(L,3);
		if(0 != easy_sockaddr_ip4(addr,ip,port)){
			return 0;
		}				
	} else if(family == AF_LOCAL) {
		const char *path = luaL_checkstring(L,2);
		if(0 != easy_sockaddr_un(addr,path)){
			return 0;
		}

	} else {
		return 0;
	}
	luaL_getmetatable(L, SOCK_ADDR_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static void register_socket(lua_State *L) {
	luaL_Reg acceptor_mt[] = {
		{"__gc", lua_acceptor_gc},
		{NULL, NULL}
	};

	luaL_Reg acceptor_methods[] = {
		{"Pause",    lua_acceptor_pause},
		{"Resume",	 lua_acceptor_resume},
		{"Close",    lua_acceptor_gc},
		{NULL,		 NULL}
	};

	luaL_Reg stream_socket_mt[] = {
		{"__gc", lua_stream_socket_gc},
		{NULL, NULL}
	};

	luaL_Reg stream_socket_methods[] = {
		{"Send",    	lua_stream_socket_send},
		{"SendUrgent",	lua_stream_socket_send_urgent},
		{"Start",   	lua_stream_socket_start},
		{"PauseRead",   lua_stream_socket_pause_read},
		{"ResumeRead",	lua_stream_socket_resume_read},		
		{"Close",   	lua_stream_socket_close},
		{"GetSockAddr", lua_stream_socket_getsockaddr},
		{"GetPeerAddr", lua_stream_socket_getpeeraddr},	
		{"SetNoDelay",  lua_stream_socket_set_nodelay},
		{"ShutDownWrite",lua_stream_socket_shutdown_write},
		{"SetCloseCallBack",lua_stream_socket_set_close_cb},
		{NULL,     		NULL}
	};

	luaL_Reg datagram_socket_mt[] = {
		{"__gc", lua_datagram_socket_gc},
		{NULL, NULL}
	};

	luaL_Reg datagram_socket_methods[] = {
		{"Sendto",    	lua_datagram_socket_sendto},
		{"Broadcast",	lua_datagram_socket_broadcast},
		{"SetBroadcast",lua_datagram_socket_set_broadcast},
		{"Start",   	lua_datagram_socket_start},
		{"Bind",   	    lua_datagram_socket_bind},		
		{"Close",   	lua_datagram_socket_close},
		{NULL,     		NULL}
	};


	luaL_newmetatable(L, ACCEPTOR_METATABLE);
	luaL_setfuncs(L, acceptor_mt, 0);

	luaL_newlib(L, acceptor_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	luaL_newmetatable(L, STREAM_SOCKET_METATABLE);
	luaL_setfuncs(L, stream_socket_mt, 0);

	luaL_newlib(L, stream_socket_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	luaL_newmetatable(L, DGRAM_SOCKET_METATABLE);
	luaL_setfuncs(L, datagram_socket_mt, 0);

	luaL_newlib(L, datagram_socket_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);


	luaL_newmetatable(L, SOCK_ADDR_METATABLE);
	lua_pop(L, 1);

	lua_newtable(L);

	lua_pushstring(L,"util");
	lua_newtable(L);
	SET_FUNCTION(L,"inet_ntop",lua_inet_ntop);
	SET_FUNCTION(L,"inet_port",lua_inet_port);
	lua_settable(L,-3);	

	lua_pushstring(L,"stream");
	lua_newtable(L);
	SET_FUNCTION(L,"socket",lua_stream_socket_new);
	SET_FUNCTION(L,"dial",lua_dail);
	SET_FUNCTION(L,"listen",lua_listen);
	SET_FUNCTION(L,"listen_ssl",lua_listen_ssl);
	lua_settable(L,-3);

	lua_pushstring(L,"datagram");
	lua_newtable(L);
	SET_FUNCTION(L,"socket",lua_datagram_socket_new);
	lua_settable(L,-3);


	SET_FUNCTION(L,"closefd",lua_close_fd);
	SET_FUNCTION(L,"addr",lua_addr);

	SET_CONST(L,AF_INET);
	SET_CONST(L,AF_LOCAL);
	SET_CONST(L,AF_INET6);	


}

