#include <assert.h>
#include "socket/connector.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"
#include "util/log.h"

int32_t 
connect_timeout(uint32_t event,uint64_t _,void *ud)
{
	if(event == TEVENT_TIMEOUT){
		connector *c = (connector*)ud;
		c->callback(-1,ETIMEDOUT,c->ud);
		close(c->fd);
		free(c);
	}
	return -1;//one shot timer,return -1
}

static int32_t 
imp_engine_add(engine *e,handle *h,
	           generic_callback callback)
{
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	connector *c = (connector*)h;
	int32_t ret = event_add(e,h,EVENT_READ) || event_add(e,h,EVENT_WRITE);
	if(ret == 0){
		h->e = e;
		c->callback = (void (*)(int32_t fd,int32_t err,void*))callback;
		if(c->timeout){
			c->t = engine_regtimer(e,c->timeout,connect_timeout,c);
		}	
	}
	return ret;
}

static void 
_process_connect(connector *c)
{
	int32_t err = 0;
	int32_t fd = -1;
	socklen_t len = sizeof(err);
	if(c->t){
		unregister_timer(c->t);
	}
	do{
		if(getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1){
			c->callback(-1,err,c->ud);
		    break;
		}
		if(err){
		    errno = err;
		    c->callback(-1,err,c->ud);    
		    break;
		}
		//success
		fd = c->fd;
	}while(0);
	event_remove((handle*)c);    
	if(fd != -1){
		c->callback(fd,0,c->ud);
	}else{
		close(c->fd);
	}		
}

static void 
process_connect(handle *h,int32_t events)
{
	if(events == EENGCLOSE){
		((connector*)h)->callback(-1,EENGCLOSE,((connector*)h)->ud);
		return;
	}
	_process_connect((connector*)h);
	free(h);
}

connector*
connector_new(int32_t fd,void *ud,uint32_t timeout)
{
	connector *c = calloc(1,sizeof(*c));
	c->fd = fd;
	c->on_events = process_connect;
	c->imp_engine_add = imp_engine_add;
	c->timeout = timeout;
	c->ud = ud;
	easy_close_on_exec(fd);
	return c;
}


#ifdef _CHUCKLUA

#define LUA_METATABLE "connector_mata"

static connector*
lua_toconnector(lua_State *L, int index) 
{
    return (connector*)luaL_testudata(L, index, LUA_METATABLE);
}

static void 
lua_process_connect(handle *h,int32_t events)
{
	_process_connect((connector*)h);
}


static int32_t 
lua_connector_new(lua_State *L)
{
	int32_t  fd;
	uint32_t timeout = 0;

	if(LUA_TNUMBER != lua_type(L,1))
		return luaL_error(L,"arg1 should be number");
	if(!lua_isnil(L,2) && LUA_TNUMBER != lua_type(L,2))
		return luaL_error(L,"arg3 should be function");	


	fd = lua_tonumber(L,1);
	if(!lua_isnil(L,2)) timeout = lua_tonumber(L,2);

	connector *c = (connector*)lua_newuserdata(L, sizeof(*c));
	memset(c,0,sizeof(*c));
	c->fd = fd;
	c->on_events = lua_process_connect;
	c->imp_engine_add = imp_engine_add;
	c->timeout = timeout;
	c->ud = c;
	easy_close_on_exec(fd);
	luaL_getmetatable(L, LUA_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static void 
luacallback(int32_t fd,int32_t err,void *ud)
{
	connector *c = (connector*)ud;
	const char *error;
	luaRef cb = c->luacallback;
	if((error = LuaCallRefFunc(cb,"ii",fd,err))){
		SYS_LOG(LOG_ERROR,"error on connector callback:%s\n",error);	
	}
	release_luaRef(&cb);
}

static int32_t 
lua_engine_add(lua_State *L)
{
	connector *c = lua_toconnector(L,1);
	engine     *e = lua_toengine(L,2);
	if(c && e){
		if(0 == imp_engine_add(e,(handle*)c,(generic_callback)luacallback)){
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


void    
reg_luaconnector(lua_State *L)
{

    luaL_Reg connector_methods[] = {
        {"Add2Engine",lua_engine_add},
        {NULL,     NULL}
    };

    luaL_newmetatable(L, LUA_METATABLE);
    luaL_newlib(L, connector_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

   	SET_FUNCTION(L,"connector",lua_connector_new);
}

#endif