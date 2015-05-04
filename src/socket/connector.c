#include <assert.h>
#include "socket/connector.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"
#include "util/log.h"

int32_t connect_timeout(uint32_t event,uint64_t _,void *ud){
	if(event == TEVENT_TIMEOUT){
		connector *c = (connector*)ud;
		c->callback(-1,ETIMEDOUT,c->ud);
		close(((handle*)c)->fd);
		free(c);
	}
	return -1;//one shot timer,return -1
}

static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback)
{
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	int32_t ret;
	connector *c = (connector*)h;
#ifdef _LINUX			
	ret = event_add(e,h,EVENT_READ | EVENT_WRITE);
#elif _BSD
	if(0 == (ret = event_add(e,h,EVENT_READ))){
		ret = event_add(e,h,EVENT_WRITE);
	}else{
		event_remove(e,h);
		return ret;
	}		
#else
		return -EUNSPPLAT;
#endif
	if(ret == 0){
		h->e = e;
		c->callback = (void (*)(int32_t fd,int32_t err,void*))callback;
		if(c->timeout){
			c->t = engine_regtimer(e,c->timeout,connect_timeout,c);
		}	
	}
	return ret;
}

static void process_connect(handle *h,int32_t events){
	connector *c = (connector*)h;
	int32_t err = 0;
	int32_t fd = -1;
	socklen_t len = sizeof(err);
	if(c->t){
		unregister_timer(c->t);
	}
	do{
		if(getsockopt(h->fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1){
			((connector*)h)->callback(-1,err,((connector*)h)->ud);
		    break;
		}
		if(err){
		    errno = err;
		    ((connector*)h)->callback(-1,err,((connector*)h)->ud);    
		    break;
		}
		//success
		fd = h->fd;
	}while(0);    
	if(fd != -1){
		event_remove(h);
		((connector*)h)->callback(fd,0,((connector*)h)->ud);
	}else{
		close(h->fd);
	}
	if(!c->luacallback.L)		
		free(h);
}


handle *connector_new(int32_t fd,void *ud,uint32_t timeout){
	connector *c = calloc(1,sizeof(*c));
	((handle*)c)->fd = fd;
	((handle*)c)->on_events = process_connect;
	((handle*)c)->imp_engine_add = imp_engine_add;
	c->timeout = timeout;
	c->ud = ud;
	easy_close_on_exec(fd);
	return (handle*)c;
}


#define LUA_METATABLE "connector_mata"

static connector *lua_toconnector(lua_State *L, int index) {
    return (connector*)luaL_testudata(L, index, LUA_METATABLE);
}

static int32_t lua_connector_del(lua_State *L){
	connector *c = lua_toconnector(L,1);
	release_luaRef(&c->luacallback);
	free(c);
	return 0;
}

static int32_t lua_connector_new(lua_State *L){
	int32_t  fd;
	uint32_t timeout = 0;

	if(LUA_TNUMBER != lua_type(L,1))
		return luaL_error(L,"arg1 should be number");
	if(!lua_isnil(L,2) && LUA_TNUMBER != lua_type(L,2))
		return luaL_error(L,"arg3 should be function");	


	fd = lua_tonumber(L,1);
	if(!lua_isnil(L,2)) timeout = lua_tonumber(L,2);

	connector *c = (connector*)lua_newuserdata(L, sizeof(*c));
	((handle*)c)->fd = fd;
	((handle*)c)->on_events = process_connect;
	((handle*)c)->imp_engine_add = imp_engine_add;
	c->timeout = timeout;
	c->ud = c;
	easy_close_on_exec(fd);

	return 1;
}

static void luacallback(int32_t fd,int32_t err,void *ud){
	connector *c = (connector*)ud;
	const char *error;
	if((error = LuaCallRefFunc(c->luacallback,"ii",fd,err))){
		SYS_LOG(LOG_ERROR,"error on connector callback:%s\n",error);	
	}
}

static int32_t lua_engine_add(lua_State *L){
	connector *c = lua_toconnector(L,1);
	engine     *e = lua_toengine(L,2);
	if(c && e){
		if(imp_engine_add(e,(handle*)c,(generic_callback)luacallback)){
			c->luacallback = toluaRef(L,3);
		}
	}
	return 0;
}

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)


void    reg_luaconnector(lua_State *L){
    luaL_Reg connector_mt[] = {
        {"__gc", lua_connector_del},
        {NULL, NULL}
    };

    luaL_Reg connector_methods[] = {
        {"Add2Engine",lua_engine_add},
        {NULL,     NULL}
    };

    luaL_newmetatable(L, LUA_METATABLE);
    luaL_setfuncs(L, connector_mt, 0);

    luaL_newlib(L, connector_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

   	SET_FUNCTION(L,"connector",lua_connector_new);
}