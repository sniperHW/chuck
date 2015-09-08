#define ACCEPTOR_METATABLE "lua_acceptor"

#define STREAM_SOCKET_METATABLE "lua_stream_socket"

//#define BYTEBUFFER_METATABLE "lua_bytebuffer"

#define lua_checkacceptor(L,I)	\
	(chk_acceptor*)luaL_checkudata(L,I,ACCEPTOR_METATABLE)

#define lua_checkstreamsocket(L,I)	\
	(chk_stream_socket*)luaL_checkudata(L,I,STREAM_SOCKET_METATABLE)

//#define lua_checkbytebuffer(L,I)	\
//	(chk_bytebuffer*)luaL_checkudata(L,I,BYTEBUFFER_METATABLE)	


static void lua_acceptor_cb(chk_acceptor *_,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	chk_luaRef   *cb = (chk_luaRef*)ud;
	const char   *error; 
	if(NULL != (error = chk_Lua_PCallRef(*cb,"ii",fd,err)))
		CHK_SYSLOG(LOG_ERROR,"error on lua_acceptor_cb %s",error);
}

static int32_t lua_acceptor_gc(lua_State *L) {
	chk_acceptor *a = lua_checkacceptor(L,1);
	chk_luaRef   *cb = (chk_luaRef*)chk_acceptor_getud(a);
	if(cb) {
		chk_luaRef_release(cb);
		free(cb);
	}
	return 0;
}

static int32_t lua_listen_ip4(lua_State *L) {
	chk_luaRef     *cb;
	chk_sockaddr    server;
	uint32_t        timeout;
	int32_t         fd,ret;
	const char     *ip;
	int16_t         port;
	lua_event_loop *event_loop;
	chk_acceptor   *a; 
	if(0 > (fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))) {
		return 0;
	}

	event_loop = lua_checkeventloop(L,1);
	ip = luaL_checkstring(L,2);
	port = (int16_t)luaL_checkinteger(L,3);
	
	if(0 != easy_sockaddr_ip4(&server,ip,port) {
		close(fd);
		return 0;
	}	

	easy_addr_reuse(fd,1);
	if(0 != easy_listen(fd,&server)){
		close(fd);
		return 0;
	}	

	if(!lua_isfunction(L,4)) 
		return luaL_error(L,"argument 4 of dail must be lua function"); 
	a   = (chk_acceptor*)lua_newuserdata(L, sizeof(*a));
	if(!a) return 0;
	cb  = calloc(sizeof(*cb));
	*cb = chk_toluaRef(L,4); 	
	chk_acceptor_init(a,fd,cb);
	luaL_getmetatable(L, ACCEPTOR_METATABLE);
	lua_setmetatable(L, -2);	
	if(0 != chk_loop_add_handle(event_loop,(chk_handle*)a,(chk_event_callback)lua_acceptor_cb))
		CHK_SYSLOG(LOG_ERROR,"event_loop add acceptor failed %s:%d",ip,port);
	return 1;
}


static void dail_ip4_cb(int32_t fd,void *ud,int32_t err) {
	chk_luaRef *cb = (chk_luaRef*)ud;
	const char *error; 
	if(NULL != (error = chk_Lua_PCallRef(*cb,"ii",fd,err))) {
		CHK_SYSLOG(LOG_ERROR,"error on dail_ip4_cb %s",error);
		close(fd);
	}	
	chk_luaRef_release(cb);
	free(cb);
}

static int32_t lua_dail_ip4(lua_State *L) {
	chk_luaRef     *cb;
	chk_sockaddr    remote;
	uint32_t        timeout;
	int32_t         fd,ret;
	const char     *ip;
	int16_t         port;
	lua_event_loop *event_loop;
	if(0 > (fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))) {
		lua_pushinteger(L,fd);
		return 1;
	}

	event_loop = lua_checkeventloop(L,1);
	ip = luaL_checkstring(L,2);
	port = (int16_t)luaL_checkinteger(L,3);
	if(!lua_isfunction(L,4)) 
		return luaL_error(L,"argument 4 of dail must be lua function"); 
	cb  = calloc(sizeof(*cb));
	*cb = chk_toluaRef(L,4); 
	timeout = (uint32_t)luaL_optinteger(L,5,0);
	ret = chk_connect(fd,&remote,NULL,event_loop->loop,dail_ip4_cb,cb,timeout);
	if(ret != 0) {
		chk_luaRef_release(cb);
		free(cb);
		lua_pushinteger(L,ret);
		return 1;
	}
	return 0;
}

static int32_t lua_stream_socket_gc(lua_State *L) {
	chk_stream_socket *s = lua_checkstreamsocket(L,1);
	chk_luaRef   *cb = (chk_luaRef*)chk_stream_socket_getUd(a);
	if(cb) {
		chk_luaRef_release(cb);
		free(cb);
	}
	chk_stream_socket_close(s);
	return 0;
}

static int32_t lua_stream_socket_new(lua_State *L) {
	chk_stream_socket *s;
	chk_stream_socket_option option;
	int32_t fd = (int32_t)luaL_checkinteger(L,1);
	option.recv_buffer_size = (uint32_t)luaL_checkinteger(L,2);
	option.decoder = lua_touserdata(L,3);
	s = (chk_stream_socket*)lua_newuserdata(L, sizeof(*s));
	if(!s) return 0;
	chk_stream_socket_init(s,fd,&option);	
	return 1;
}

/*
static int32_t lua_bytebuffer_gc(lua_State *L) {
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	chk_bytebuffer_finalize(b);
	return 0;
}

static void _new_bytebuffer(chk_bytebuffer *o) {
	chk_bytebuffer *b = (chk_bytebuffer*)lua_newuserdata(L, sizeof(*b));
	if(b) {
		chk_bytebuffer_init(b,o);
		luaL_getmetatable(L, BYTEBUFFER_METATABLE);
		lua_setmetatable(L, -2);	
	}else
		lua_pushnil(L);
}

static int32_t lua_new_bytebuffer(lua_State *L) {

}

struct buffer_pusher {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
	chk_bytebuffer *buff;
};


static void Push_Buffer(chk_luaPushFunctor *_,lua_State *L) {
	struct buffer_pusher *self = (struct buffer_pusher*)_;
	_new_bytebuffer(self->buff);	
}
*/

static void data_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	/*chk_luaRef   *cb = (chk_luaRef*)chk_stream_socket_getUd(s);
	const char   *error; 
	if(NULL != (error = chk_Lua_PCallRef(*cb,"ii",fd,err)))
		CHK_SYSLOG(LOG_ERROR,"error on data_cb %s",error);*/
}

static int32_t lua_stream_socket_bind(lua_State *L) {
	chk_stream_socket *s;
	chk_event_loop    *event_loop;
	chk_luaRef        *cb;
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of stream_socket_bind must be lua function");

	event_loop = lua_checkeventloop(L,1);
	s = lua_checkstreamsocket(L,2);
	cb = calloc(sizeof(*cb));
	*cb = chk_toluaRef(L,3);
	if(0 != chk_loop_add_handle(event_loop,(chk_handle*)s,(chk_event_callback)data_cb)) {
		lua_pushstring(L,"stream_socket_bind failed");
		return 1;
	}
	return 0;
}


static void register_socket(lua_State *L) {
	
}

