#include <assert.h>
#include <sys/signalfd.h>
#include "util/signaler.h"
#include "engine/engine.h"
#include "util/log.h"    


static int32_t 
imp_engine_add(engine *e,handle *h,
               generic_callback callback)
{
    assert(e && h && callback);
    if(h->e) return -EASSENG;
    int32_t ret = event_add(e,h,EVENT_READ);
    if(ret == 0){
        ((signaler*)h)->callback = (void (*)(struct signaler *,int32_t,void *ud))callback;
    }
    return ret;
}

static void 
on_signal(handle *h,int32_t events)
{

    struct  signalfd_siginfo fdsi;
    int32_t  fd = h->fd;
    signaler *s = (signaler*)h;
    int32_t ret = TEMP_FAILURE_RETRY(read(fd, &fdsi, sizeof(fdsi)));
    if(ret != sizeof(fdsi))
        return;    

    s->callback(s,s->signum,s->ud);
}


static int32_t 
signaler_init(int32_t signum)
{
    sigset_t  mask;  
    sigemptyset(&mask);
    sigaddset(&mask, signum);
    
    if(sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
        return -errno;  

    return signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);    
}


signaler*
signaler_new(int32_t signum,void *ud)
{
    
    int32_t fd = signaler_init(signum);
    if(fd < 0) return NULL;
    
    signaler *s = calloc(1,sizeof(*s));
    ((handle*)s)->fd = fd;
    s->signum = signum;
    s->ud = ud;
    ((handle*)s)->on_events = on_signal;
    ((handle*)s)->imp_engine_add = imp_engine_add;    
    return s;
}

void 
signaler_del(signaler *s)
{
    close(((handle*)s)->fd);
    free(s);
}


#define LUA_METATABLE "signer_mata"

static signaler*
lua_tosignaler(lua_State *L, int index) 
{
    return (signaler*)luaL_testudata(L, index, LUA_METATABLE);
}

static int32_t 
lua_signaler_gc(lua_State *L)
{
    signaler *s = lua_tosignaler(L,1);
    release_luaRef(&s->luacallback);
    close(((handle*)s)->fd);
    return 0;
}

static void 
luacallback(signaler *s,int32_t signum,void *ud)
{
    const char *error;
    if((error = LuaCallRefFunc(s->luacallback,"i",signum))){
        SYS_LOG(LOG_ERROR,"error on signaler callback:%s\n",error); 
    }
}

static int32_t 
lua_engine_add(lua_State *L)
{
    signaler   *s = lua_tosignaler(L,1);
    engine     *e = lua_toengine(L,2);
    if(s && e){
        if(0 == imp_engine_add(e,(handle*)s,(generic_callback)luacallback)){
            s->luacallback = toluaRef(L,3);
        }
    }
    return 0;
}

static int32_t 
lua_engine_remove(lua_State *L)
{
    signaler   *s = lua_tosignaler(L,1);
    if(s)
        engine_remove((handle*)s);
    return 0;
}

static int32_t 
lua_signaler_new(lua_State *L)
{

    if(LUA_TNUMBER != lua_type(L,1))
        return luaL_error(L,"arg1 should be number");
    int32_t signum = lua_tointeger(L,1);
    int32_t fd = signaler_init(signum);
    if(fd < 0){
        lua_pushnil(L);
        return 1;
    }
    
    signaler *s = lua_newuserdata(L, sizeof(*s));
    memset(s,0,sizeof(*s));
    ((handle*)s)->fd = fd;
    s->signum = signum;
    ((handle*)s)->on_events = on_signal;
    ((handle*)s)->imp_engine_add = imp_engine_add;
    luaL_getmetatable(L, LUA_METATABLE);
    lua_setmetatable(L, -2);  
    return 1;
}


#define SET_FUNCTION(L,NAME,FUNC) do{\
    lua_pushstring(L,NAME);\
    lua_pushcfunction(L,FUNC);\
    lua_settable(L, -3);\
}while(0)

#define SET_CONST(L,N) do{\
        lua_pushstring(L, #N);\
        lua_pushinteger(L, N);\
        lua_settable(L, -3);\
}while(0)

void    
reg_luasignaler(lua_State *L)
{
    luaL_Reg signaler_mt[] = {
        {"__gc", lua_signaler_gc},
        {NULL, NULL}
    };

    luaL_Reg signaler_methods[] = {
        {"Register",lua_engine_add},
        {"Remove", lua_engine_remove},
        {NULL,     NULL}
    };

    luaL_newmetatable(L, LUA_METATABLE);
    luaL_setfuncs(L, signaler_mt, 0);

    luaL_newlib(L, signaler_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    lua_newtable(L);

    SET_CONST(L,SIGTERM);
    SET_CONST(L,SIGINT);
    SET_FUNCTION(L,"signaler",lua_signaler_new);
}