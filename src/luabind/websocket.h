#define WEBSOCKET_METATABLE "lua_websocket"

typedef struct {
	chk_stream_socket    *socket;
	chk_luaRef            cb;
}websocket;

#define lua_check_websocket(L,I)	\
	(websocket*)luaL_checkudata(L,I,WEBSOCKET_METATABLE)


static int lua_upgrade(lua_State *L) {
	http_connection   *conn = lua_check_http_connection(L,1);
	if(conn->socket) {
		websocket *ws = LUA_NEWUSERDATA(L,websocket);
		if(!ws) {
			CHK_SYSLOG(LOG_ERROR,"LUA_NEWUSERDATA() failed");			
			return 0;
		}
		conn->socket = NULL;
		chk_stream_socket_setUd(s,chk_ud_make_void(ws));
		luaL_getmetatable(L, WEBSOCKET_METATABLE);
		lua_setmetatable(L, -2);
		return 1;

	} else {
		return 0;
	}
}

static void register_websocket(lua_State *L) {
	luaL_Reg websocket_mt[] = {
		{"__gc", lua_websocket_gc},
		{NULL, NULL}
	};

	luaL_Reg websocket_methods[] = {															
		{NULL,     NULL}
	};

	luaL_newmetatable(L, WEBSOCKET_METATABLE);
	luaL_setfuncs(L, websocket_mt, 0);

	luaL_newlib(L, websocket_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	
	lua_newtable(L);	
	SET_FUNCTION(L,"Upgrade",lua_upgrade);
}