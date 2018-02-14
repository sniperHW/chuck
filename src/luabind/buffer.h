#define BYTEBUFFER_METATABLE "lua_bytebuffer"

#define lua_checkbytebuffer(L,I)	\
	(chk_bytebuffer*)luaL_checkudata(L,I,BYTEBUFFER_METATABLE)

static int32_t lua_bytebuffer_gc(lua_State *L) {
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	chk_bytebuffer_finalize(b);
	return 0;
}

typedef struct {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
	chk_bytebuffer *data;
}luaBufferPusher;

static void PushBuffer(chk_luaPushFunctor *_,lua_State *L) {
	luaBufferPusher *self = (luaBufferPusher*)_;
	chk_bytebuffer *data = LUA_NEWUSERDATA(L,chk_bytebuffer);
	if(data) {
		chk_bytebuffer_init(data,self->data->head,self->data->spos,self->data->datasize,self->data->flags);
		luaL_getmetatable(L, BYTEBUFFER_METATABLE);
		lua_setmetatable(L, -2);
	} else {
		CHK_SYSLOG(LOG_ERROR,"newuserdata() chk_bytebuffer failed");	
		lua_pushnil(L);
	}
}

static int32_t lua_new_bytebuffer(lua_State *L) {
	chk_bytebuffer *b;
	chk_bytechunk  *chunk;
	size_t size = 0;
	const char *str = NULL;



	if(lua_type(L,1) == LUA_TSTRING) {
		str = lua_tolstring(L,1,&size);
		b = LUA_NEWUSERDATA(L,chk_bytebuffer);		
		if(!b){	
			CHK_SYSLOG(LOG_ERROR,"newuserdata() chk_bytebuffer failed");
			return 0;
		}

		chunk = chk_bytechunk_new((void*)str,(uint32_t)size);

		if(!chunk) {
			CHK_SYSLOG(LOG_ERROR,"chk_bytechunk_new() failed:%d",size);			
			return 0;
		}
		chk_bytebuffer_init(b,chunk,0,(uint32_t)size,0);
		chk_bytechunk_release(chunk);
	}
	else {
		size = (uint32_t)luaL_optinteger(L,1,64);
		b = LUA_NEWUSERDATA(L,chk_bytebuffer);
		if(!b){	
			CHK_SYSLOG(LOG_ERROR,"newuserdata() chk_bytebuffer failed");
			return 0;
		}
		chk_bytebuffer_init(b,NULL,0,size,0);
	}
	luaL_getmetatable(L, BYTEBUFFER_METATABLE);
	lua_setmetatable(L, -2);	
	return 1;		
}

static int32_t lua_bytebuffer_clone(lua_State *L) {
	chk_bytebuffer *self,*o;
	self = lua_checkbytebuffer(L,1);

	o = LUA_NEWUSERDATA(L,chk_bytebuffer);
	
	if(!o){	
		CHK_SYSLOG(LOG_ERROR,"newuserdata() chk_bytebuffer failed");
		return 0;
	}

	if(o != chk_bytebuffer_share(o,self)) {
		CHK_SYSLOG(LOG_ERROR,"chk_bytebuffer_share() failed");		
		return 0;
	}

	luaL_getmetatable(L, BYTEBUFFER_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}


static int32_t lua_bytebuffer_read(lua_State *L) {
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	uint32_t size     = (uint32_t)luaL_checkinteger(L,2);
	luaL_Buffer     lb;
	char           *in;

	if(!b->head || b->datasize < size) {
		lua_pushnil(L);
		return 1;
	}

	in = luaL_buffinitsize(L,&lb,(size_t)size);
	chk_bytebuffer_read_drain(b,in,size);
	luaL_pushresultsize(&lb,(size_t)size);	
	return 1;	
}

static int32_t lua_bytebuffer_data(lua_State *L) {
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	luaL_Buffer     lb;
	char           *in;

	if(!b->head) {
		lua_pushnil(L);
		return 1;
	}

	if(b->head->cap - b->spos >= b->datasize) {
		//数据在唯一的chunk中
		lua_pushlstring(L,(const char *)&(b->head->data[b->spos]),(size_t)b->datasize);
	}else {
		//数据跨越chunk	
		in = luaL_buffinitsize(L,&lb,(size_t)b->datasize);
		chk_bytebuffer_read(b,0,in,b->datasize);
		luaL_pushresultsize(&lb,(size_t)b->datasize);	
	}
	return 1;
}

static int32_t lua_bytebuffer_append_string(lua_State *L) {
	const char *str;
	size_t len = 0;
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	str = lua_tolstring(L,2,&len);

	do{
		if(str && len > 0) {
			if(0 != chk_bytebuffer_append(b,(uint8_t*)str,(uint32_t)len)){
				CHK_SYSLOG(LOG_ERROR,"chk_bytebuffer_append() failed");
				break;
			}
			return 0;
		}

	}while(0);
	lua_pushstring(L,"append string failed");
	return 1;
}

static int32_t lua_bytebuffer_size(lua_State *L) {
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	lua_pushinteger(L,b->datasize);
	return 1;
}


static void register_buffer(lua_State *L) {

	luaL_Reg bytebuffer_mt[] = {
		{"__gc", lua_bytebuffer_gc},
		{NULL, NULL}
	};

	luaL_Reg bytebuffer_methods[] = {
		{"Clone",    lua_bytebuffer_clone},
		{"Content",  lua_bytebuffer_data},
		{"Read",lua_bytebuffer_read},
		{"AppendStr",lua_bytebuffer_append_string},
		{"Size",lua_bytebuffer_size},
		{NULL,     NULL}
	};	

	luaL_newmetatable(L, BYTEBUFFER_METATABLE);
	luaL_setfuncs(L, bytebuffer_mt, 0);

	luaL_newlib(L, bytebuffer_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_newtable(L);
	SET_FUNCTION(L,"New",lua_new_bytebuffer);
}	