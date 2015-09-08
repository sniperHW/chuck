#define ACCEPTOR_METATABLE "lua_acceptor"

#define STREAM_SOCKET_METATABLE "lua_stream_socket"

#define lua_checkacceptor(L,I)	\
	(chk_acceptor*)luaL_checkudata(L,I,ACCEPTOR_METATABLE)

#define lua_checkstreamsocket(L,I)	\
	(chk_stream_socket*)luaL_checkudata(L,I,STREAM_SOCKET_METATABLE)


static void lua_acceptor_cb(chk_acceptor *_,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	chk_luaRef   *cb = (chk_luaRef*)ud;
	const char   *error; 
	if(NULL != (error = chk_Lua_PCallRef(*cb,"ii",fd,err))) {
		close(fd);
		CHK_SYSLOG(LOG_ERROR,"error on lua_acceptor_cb %s",error);
	}
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

static int32_t lua_acceptor_pause(lua_State *L) {
	chk_acceptor *a = lua_checkacceptor(L,1);
	chk_acceptor_pause(a);
	return 0;
}

static int32_t lua_acceptor_resume(lua_State *L) {
	chk_acceptor *a = lua_checkacceptor(L,1);
	chk_acceptor_resume(a);
	return 0;
}

static int32_t lua_listen_ip4(lua_State *L) {
	chk_luaRef     *cb;
	chk_sockaddr    server;
	int32_t         fd;
	const char     *ip;
	int16_t         port;
	chk_event_loop *event_loop;
	chk_acceptor   *a; 
	if(0 > (fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))) {
		return 0;
	}

	event_loop = lua_checkeventloop(L,1);
	ip = luaL_checkstring(L,2);
	port = (int16_t)luaL_checkinteger(L,3);
	
	if(0 != easy_sockaddr_ip4(&server,ip,port)) {
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
	cb  = calloc(1,sizeof(*cb));
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
		close(fd);
		CHK_SYSLOG(LOG_ERROR,"error on dail_ip4_cb %s",error);
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
	chk_event_loop *event_loop;

	if(!lua_isfunction(L,4)) { 
		return luaL_error(L,"argument 4 of dail must be lua function");
	}

	if(0 > (fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))) {
		lua_pushstring(L,"create socket error");
		return 1;
	}

	event_loop = lua_checkeventloop(L,1);
	ip = luaL_checkstring(L,2);
	port = (int16_t)luaL_checkinteger(L,3);	

	if(0 != easy_sockaddr_ip4(&remote,ip,port)) {
		close(fd);
		lua_pushstring(L,"lua_dail_ip4 invaild address or port");
		return 1;
	}

	cb  = calloc(1,sizeof(*cb));
	*cb = chk_toluaRef(L,4); 
	timeout = (uint32_t)luaL_optinteger(L,5,0);
	ret = chk_connect(fd,&remote,NULL,event_loop,dail_ip4_cb,cb,timeout);
	if(ret != 0) {
		chk_luaRef_release(cb);
		free(cb);
		lua_pushstring(L,"connect error");
		return 1;
	}
	return 0;
}

static int32_t lua_stream_socket_gc(lua_State *L) {
	chk_stream_socket *s = lua_checkstreamsocket(L,1);
	chk_luaRef   *cb = (chk_luaRef*)chk_stream_socket_getUd(s);
	if(cb) {
		chk_luaRef_release(cb);
		free(cb);
	}
	chk_stream_socket_close(s,1);
	return 0;
}

static void data_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	chk_luaRef *cb = (chk_luaRef*)chk_stream_socket_getUd(s);
	luaBufferPusher pusher = {PushBuffer,data};
	const char *error_str;
	if(data) error_str = chk_Lua_PCallRef(*cb,"fi",(chk_luaPushFunctor*)&pusher,error);
	else error_str = chk_Lua_PCallRef(*cb,"pi",NULL,error);
	if(error_str) CHK_SYSLOG(LOG_ERROR,"error on data_cb %s",error_str);	
}

static int32_t lua_stream_socket_bind(lua_State *L) {
	chk_stream_socket *s;
	chk_event_loop    *event_loop;
	chk_luaRef        *cb;
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of stream_socket_bind must be lua function");
	s = lua_checkstreamsocket(L,1);
	event_loop = lua_checkeventloop(L,2);

	cb = calloc(1,sizeof(*cb));
	*cb = chk_toluaRef(L,3);
	chk_stream_socket_setUd(s,cb);
	if(0 != chk_loop_add_handle(event_loop,(chk_handle*)s,(chk_event_callback)data_cb)) {
		lua_pushstring(L,"stream_socket_bind failed");
		return 1;
	}
	return 0;
}

static int32_t lua_stream_socket_new(lua_State *L) {
	int32_t fd;
	chk_stream_socket *s;
	chk_stream_socket_option option = {
		.decoder = NULL
	};
	fd = (int32_t)luaL_checkinteger(L,1);
	option.recv_buffer_size = (uint32_t)luaL_optinteger(L,2,4096);
	if(lua_islightuserdata(L,3)) option.decoder = lua_touserdata(L,3);
	s = (chk_stream_socket*)lua_newuserdata(L, sizeof(*s));
	chk_stream_socket_init(s,fd,&option);
	luaL_getmetatable(L, STREAM_SOCKET_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}	

static int32_t lua_stream_socket_close(lua_State *L) {
	chk_stream_socket *s = lua_checkstreamsocket(L,1);
	chk_stream_socket_close(s,0);
	return 0;
}

static int32_t lua_stream_socket_pause(lua_State *L) {
	chk_stream_socket *s = lua_checkstreamsocket(L,1);
	chk_stream_socket_pause(s);
	return 0;
}

static int32_t lua_stream_socket_resume(lua_State *L) {
	chk_stream_socket *s = lua_checkstreamsocket(L,1);
	chk_stream_socket_resume(s);
	return 0;
}

static int32_t lua_close_fd(lua_State *L) {
	int32_t fd = (int32_t)luaL_checkinteger(L,1);
	close(fd);
	return 0;
}

static int32_t lua_stream_socket_send(lua_State *L) {
	chk_bytebuffer    *b,*o;
	chk_stream_socket *s = lua_checkstreamsocket(L,1);
	o = lua_checkbytebuffer(L,2);
	b = chk_bytebuffer_clone(NULL,o);
	if(0 != chk_stream_socket_send(s,b)){
		lua_pushstring(L,"send error");
		return 1;
	}
	return 0;
}

static void register_socket(lua_State *L) {
	luaL_Reg acceptor_mt[] = {
		{"__gc", lua_acceptor_gc},
		{NULL, NULL}
	};

	luaL_Reg acceptor_methods[] = {
		{"Pause",    lua_acceptor_pause},
		{"Resume",	 lua_acceptor_resume},
		{NULL,     NULL}
	};

	luaL_Reg stream_socket_mt[] = {
		{"__gc", lua_stream_socket_gc},
		{NULL, NULL}
	};

	luaL_Reg stream_socket_methods[] = {
		{"Send",    lua_stream_socket_send},
		{"Bind",    lua_stream_socket_bind},
		{"Pause",   lua_stream_socket_pause},
		{"Resume",	lua_stream_socket_resume},		
		{"Close",   lua_stream_socket_close},
		{NULL,     NULL}
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

	lua_newtable(L);

	lua_pushstring(L,"stream");
	lua_newtable(L);
	
	SET_FUNCTION(L,"New",lua_stream_socket_new);

	lua_pushstring(L,"ip4");
	lua_newtable(L);
	SET_FUNCTION(L,"dail",lua_dail_ip4);
	SET_FUNCTION(L,"listen",lua_listen_ip4);
	lua_settable(L,-3);

	lua_settable(L,-3);

	SET_FUNCTION(L,"closefd",lua_close_fd);


}

