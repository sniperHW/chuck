#include <assert.h>
#include "socket/acceptor.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"
#include "util/log.h"

static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback)
{
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	int32_t ret = event_add(e,h,EVENT_READ);
	if(ret == 0){
		easy_noblock(h->fd,1);
		((acceptor*)h)->callback = (void (*)(int32_t fd,sockaddr_*,void*))callback;
	}
	return ret;
}


static int _accept(handle *h,sockaddr_ *addr){
	socklen_t len = 0;
	int32_t fd; 
	while((fd = accept(h->fd,(struct sockaddr*)addr,&len)) < 0){
#ifdef EPROTO
		if(errno == EPROTO || errno == ECONNABORTED)
#else
		if(errno == ECONNABORTED)
#endif
			continue;
		else
			return -errno;
	}
	return fd;
}

static void process_accept(handle *h,int32_t events){
    int fd;
    sockaddr_ addr;
    for(;;){
    	fd = _accept(h,&addr);
    	if(fd < 0)
    	   break;
    	else{
		   ((acceptor*)h)->callback(fd,&addr,((acceptor*)h)->ud);
    	}      
    }
}

handle *acceptor_new(int32_t fd,void *ud){
	acceptor *a = calloc(1,sizeof(*a));
	a->ud = ud;
	((handle*)a)->fd = fd;
	((handle*)a)->on_events = process_accept;
	((handle*)a)->imp_engine_add = imp_engine_add;
	easy_close_on_exec(fd);
	return (handle*)a;
}

void    acceptor_del(handle *h){
	close(h->fd);
	free(h);
}


#define LUA_METATABLE "acceptor_mata"

static acceptor *lua_toacceptor(lua_State *L, int index) {
    return (acceptor*)luaL_testudata(L, index, LUA_METATABLE);
}

static int32_t lua_acceptor_del(lua_State *L){
	acceptor *a = lua_toacceptor(L,1);
	release_luaRef(&a->luacallback);
	acceptor_del((handle*)a);
	return 0;
}

static int32_t lua_acceptor_new(lua_State *L){
	int32_t  fd;
	if(LUA_TNUMBER != lua_type(L,1))
		return luaL_error(L,"arg1 should be number");
	fd = lua_tonumber(L,1);
	acceptor *a = (acceptor*)lua_newuserdata(L, sizeof(*a));
	((handle*)a)->fd = fd;
	((handle*)a)->on_events = process_accept;
	((handle*)a)->imp_engine_add = imp_engine_add;
	a->ud = a;
	easy_close_on_exec(fd);
	return 1;
}

static void luacallback(int32_t fd,sockaddr_ *addr,void *ud){
	acceptor *a = (acceptor*)ud;
	const char *error;
	if((error = LuaCallRefFunc(a->luacallback,"i",fd))){
		SYS_LOG(LOG_ERROR,"error on acceptor callback:%s\n",error);	
	}
}

static int32_t lua_engine_add(lua_State *L){
	acceptor   *a = lua_toacceptor(L,1);
	engine     *e = lua_toengine(L,2);
	if(a && e){
		if(imp_engine_add(e,(handle*)a,(generic_callback)luacallback)){
			a->luacallback = toluaRef(L,3);
		}
	}
	return 0;
}

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)

void    reg_luaacceptor(lua_State *L){
    luaL_Reg acceptor_mt[] = {
        {"__gc", lua_acceptor_del},
        {NULL, NULL}
    };

    luaL_Reg acceptor_methods[] = {
        {"Add2Engine",lua_engine_add},
        {NULL,     NULL}
    };

    luaL_newmetatable(L, LUA_METATABLE);
    luaL_setfuncs(L, acceptor_mt, 0);

    luaL_newlib(L, acceptor_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

   	SET_FUNCTION(L,"acceptor",lua_acceptor_new);
} 