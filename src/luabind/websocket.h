#define WEBSOCKET_METATABLE "lua_websocket"

typedef struct {
	chk_stream_socket    *socket;
	chk_luaRef            cb;
} websocket;

#define lua_check_websocket(L,I)	\
	(websocket*)luaL_checkudata(L,I,WEBSOCKET_METATABLE)

typedef struct websocket_decoder {
	void (*update)(chk_decoder*,chk_bytechunk *b,uint32_t spos,uint32_t size);
	chk_bytebuffer *(*unpack)(chk_decoder*,int32_t *err);
	void (*release)(chk_decoder*);
	uint32_t       spos;
	uint32_t       size;
	chk_bytechunk *b;
}websocket_decoder;

static void websocket_update(chk_decoder *d,chk_bytechunk *b,uint32_t spos,uint32_t size) {
}

static chk_bytebuffer *websocket_unpack(chk_decoder *d,int32_t *err) {
	return NULL;
}

static void release_decoder(chk_decoder *_) {
	websocket_decoder *d = ((websocket_decoder*)_);
	//if(d->b) chk_bytechunk_release(d->b);
	free(d);
}

static websocket_decoder *new_websocket_decoder() {
	websocket_decoder *d = calloc(1,sizeof(*d));
	if(!d){
		CHK_SYSLOG(LOG_ERROR,"calloc websocket_decoder failed");		 
		return NULL;
	}
	d->update   = websocket_update;
	d->unpack   = websocket_unpack;
	d->release  = release_decoder;
	return d;
}	

void chk_stream_socket_set_decoder(chk_stream_socket *s,chk_decoder *decoder);
static int lua_upgrade(lua_State *L) {
	http_connection   *conn = lua_check_http_connection(L,1);
	if(conn->socket) {
		websocket *ws = LUA_NEWUSERDATA(L,websocket);
		if(!ws) {
			CHK_SYSLOG(LOG_ERROR,"LUA_NEWUSERDATA() failed");			
			return 0;
		}
		websocket_decoder *decoder = new_websocket_decoder();
		if(NULL == decoder) {
			CHK_SYSLOG(LOG_ERROR,"new_websocket_decoder() failed");			
			return 0;
		}
		ws->socket = conn->socket;
		release_lua_http_parser(conn->parser);
		chk_luaRef_release(&conn->cb);
		conn->socket = NULL;
		//从event_loop移除，待用户调用start后重新绑定
		chk_loop_remove_handle((chk_handle*)ws->socket);
		//将decoder替换成websocket_decoder
		chk_stream_socket_set_decoder(ws->socket,(chk_decoder*)decoder);
		//http_connection的parser是通过close_callback释放的，这里已经提前释放所以需要把close_callback设置为空
		chk_stream_socket_set_close_callback(ws->socket,NULL,chk_ud_make_void(NULL));
		chk_stream_socket_setUd(ws->socket,chk_ud_make_void(ws));
		luaL_getmetatable(L, WEBSOCKET_METATABLE);
		lua_setmetatable(L, -2);
		return 1;
	} else {
		return 0;
	}
}

static int lua_websocket_gc(lua_State *L) {
	//websocket *ws = lua_check_websocket(L,1);
	return 0;
}

static int lua_websocket_send(lua_State *L) {
	return 0;
}

static int lua_websocket_close(lua_State *L) {
	return 0;
}

static int lua_websocket_bind(lua_State *L) {
	return 0;
}

static int lua_websocket_set_close_cb(lua_State *L) {
	return 0;
}

static int lua_websocket_getsockaddr(lua_State *L) {
	return 0;
}

static int lua_websocket_getpeeraddr(lua_State *L) {
	return 0;	
}

static void register_websocket(lua_State *L) {
	luaL_Reg websocket_mt[] = {
		{"__gc", lua_websocket_gc},
		{NULL, NULL}
	};

	luaL_Reg websocket_methods[] = {
		{"Send",    		lua_websocket_send},
		{"Start",   		lua_websocket_bind},		
		{"Close",   		lua_websocket_close},
		{"GetSockAddr", 	lua_websocket_getsockaddr},
		{"GetPeerAddr", 	lua_websocket_getpeeraddr},	
		{"SetCloseCallBack",lua_websocket_set_close_cb},															
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