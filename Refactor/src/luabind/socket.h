static void dail_ip4_cb(int32_t fd,int32_t err,void *ud) {
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

