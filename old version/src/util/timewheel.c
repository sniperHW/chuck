#include <assert.h>
#include <stdio.h>
#include "util/time.h"
#include "util/timewheel.h"
#include "util/dlist.h"
#include "util/log.h"

enum{
	wheel_sec = 0,  
	wheel_hour,     
	wheel_day,      
};

typedef struct {
	int8_t   type;
	int16_t  cur;
	dlist    timer_list[0]; 
}wheel;

#define wheel_size(T) (T==wheel_sec?1000:T==wheel_hour?3600:T==wheel_day?24:0)
#define precision(T) (T==wheel_sec?1:T==wheel_hour?1000:T==wheel_day?3600:0)

#ifdef TEST_REWINDING    //测试时间回绕
# define REWINDING_TIME   (1000*3600*24)
# define CAL_EXPIRE(T,MS) (((T) + (MS)) % REWINDING_TIME)
# define INC_LASTTICK(T)  ((T) = ((T) + 1) % REWINDING_TIME)
#else
# define REWINDING_TIME   0xFFFFFFFFFFFFFFFF
# define CAL_EXPIRE(T,MS) ((T) + (MS))
# define INC_LASTTICK(T)  (++(T))
#endif

static wheel *wheel_new(uint8_t type)
{
	wheel   *w;
	int16_t  size,i;
	if(type >  wheel_day) return NULL;
	w       = calloc(1,sizeof(*w)+(wheel_size(type)*sizeof(dlist)));	
	w->type = type;
	w->cur  = type == wheel_sec ? -1:0;
	size    = cast(uint16_t,wheel_size(type));
	for(i = 0; i < size; ++i) dlist_init(&w->timer_list[i]);
	return w;	
}

enum{
	INCB = 1,
	RELEASING = 1 << 1,
};

struct timer{
	dlistnode     node;
	uint32_t      timeout;
	uint64_t      expire;
	int32_t       status;
	timer_cb      cb; 
	void         *ud;
#ifdef _CHUCKLUA    
    luaRef        luacb;
#endif	
};

struct wheelmgr{
	wheel 		*wheels[wheel_day+1];
	uint64_t    *ptrtick;
	uint64_t     lasttick;
};

//将timer插入到合适的链表中
static inline void add2wheel(wheelmgr *m,wheel *w,timer *t,uint64_t remain) {
	int16_t i;
	uint64_t wsize = wheel_size(w->type);
	if(w->type == wheel_day || wsize >= remain) {
		i = (w->cur + remain)%wsize;
		dlist_pushback(&w->timer_list[i],cast(dlistnode*,t));	
	}else {
		//插入到上一级时间轮中
		remain -= 1;
		remain /= wsize;
		return add2wheel(m,m->wheels[w->type+1],t,remain);		
	}
}


static inline uint64_t cal_remain(uint64_t now,uint64_t expire) {
	if(now >= expire)
		//出现时间回绕
		return (REWINDING_TIME - now) + expire;
	else return expire - now;
}

static inline void _reg(wheelmgr *m,timer *t,uint64_t tick,wheel *w) {
	add2wheel(m,w?w:m->wheels[wheel_sec],t,cal_remain(tick,t->expire));
}

static void fire(wheelmgr *m,wheel *w,uint64_t tick) {
	int32_t    ret;
	timer 	  *t;
	dlist      timer_list;	
	w->cur = (w->cur+1)%wheel_size(w->type);
	if(!dlist_empty(&w->timer_list[w->cur])) {
		dlist_init(&timer_list);
		dlist_move(&timer_list,&w->timer_list[w->cur]);				
		if(w->type == wheel_sec) {		
			while((t = cast(timer*,dlist_pop(&timer_list)))) {
				t->status |= INCB;
				ret = t->cb(TEVENT_TIMEOUT,tick,t->ud);
				t->status ^= INCB;
				if(!(t->status & RELEASING) && ret >= 0) {
					if(ret > 0) t->timeout = ret;
					t->expire = CAL_EXPIRE(tick,t->timeout);
					_reg(m,t,tick,NULL);
				}else {
#if _CHUCKLUA
						release_luaRef(&t->luacb);
#else			
						t->cb(TEVENT_DESTROY,t->expire,t->ud);
						free(t);
#endif					
				}
			}		
		}else {		
			while((t = cast(timer*,dlist_pop(&timer_list))))
				_reg(m,t,tick,NULL);
		}
	}
	if(w->cur + 1 == wheel_size(w->type) && w->type < wheel_day)
		fire(m,m->wheels[w->type+1],tick);
}

void wheelmgr_tick(wheelmgr *m,uint64_t now)
{
	if(!m->ptrtick) return;//没有注册过定时器
	while(m->lasttick != now) {
		INC_LASTTICK(m->lasttick);
		fire(m,m->wheels[wheel_sec],m->lasttick);
	}
} 

timer *wheelmgr_register(wheelmgr *m,uint32_t ms,timer_cb cb,void*ud,uint64_t now)
{
	timer *t;
	if(!cb) return NULL;
	t = calloc(1,sizeof(*t));
	t->timeout = ms > MAX_TIMEOUT ? MAX_TIMEOUT : (ms > 0 ? ms : 1);
	t->cb = cb;
	t->ud = ud;
	if(!m->ptrtick){
		m->ptrtick  = &m->lasttick;
		m->lasttick = now;
	}
	t->expire = CAL_EXPIRE(now,t->timeout);
	_reg(m,t,m->lasttick,NULL);
	return t;	
}

wheelmgr *wheelmgr_new()
{
	int i;
	wheelmgr *t = calloc(1,sizeof(*t));
	for(i = 0; i < wheel_day+1; ++i)
		t->wheels[i] = wheel_new(i);
	return t;
}

void unregister_timer(timer *t)
{
	if(t->status & RELEASING)
		return;
	t->status |= RELEASING;
	if(!(t->status & INCB)){
		dlist_remove(cast(dlistnode*,t));
		t->cb(TEVENT_DESTROY,t->expire,t->ud);
		free(t);
	}
}

void wheelmgr_del(wheelmgr *m)
{
	int i;
	int16_t j,size;
	dlist   *items;
	timer   *t;
	for(i = 0; i < wheel_day+1; ++i){
		size = wheel_size(m->wheels[i]->type);
		for(j = 0; j < size; ++j){
			items = &m->wheels[i]->timer_list[j];
			while((t = cast(timer*,dlist_pop(items)))){
				t->cb(TEVENT_DESTROY,t->expire,t->ud);
				free(t);				
			}
		}
		free(m->wheels[i]);
	}
	free(m);	
}

uint64_t timer_expire(timer *t) {
	return t->expire; 
}

uint32_t timer_timeout(timer *t) {
	return t->timeout;
}


#ifdef _CHUCKLUA

#include "lua/lua_util.h"


#define TIMER_METATABLE     "timer_meta"
#define WHEELMGR_METATABLE  "wheelmgr_meta"

static int32_t lua_wheelmgr_new(lua_State *L)
{
	int i;
	wheelmgr *w = cast(wheelmgr*,lua_newuserdata(L, sizeof(*w)));	
	memset(w,0,sizeof(*w));
	for(i = 0; i < wheel_day+1; ++i)
		w->wheels[i] = wheel_new(i);
	luaL_getmetatable(L, WHEELMGR_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

wheelmgr *lua_towheelmgr(lua_State *L, int index)
{
    return (wheelmgr*)luaL_testudata(L, index, WHEELMGR_METATABLE);
}

timer *lua_totimer(lua_State *L, int index)
{
    return (timer*)luaL_testudata(L, index, TIMER_METATABLE);
}

static int32_t lua_wheelmgr_gc(lua_State *L)
{
	wheelmgr *w = lua_towheelmgr(L,1);
	int      i;
	int16_t  j,size;
	dlist   *items;
	timer   *t;
	for(i = 0; i < wheel_day+1; ++i){
		size = wheel_size(w->wheels[i]->type);
		for(j = 0; j < size; ++j){
			items = &w->wheels[i]->timer_list[j];
			while((t = cast(timer*,dlist_pop(items)))){
				release_luaRef(&t->luacb);				
			}
		}
		free(w->wheels[i]);
	}
	return 0;
}

static int32_t lua_timeout_callback(uint32_t _1,uint64_t _2,void *ud)
{
	luaRef     *cb = cast(luaRef*,ud);
	const char *error;
	lua_Integer ret = -1;
	if((error = LuaCallRefFunc(*cb,":i",&ret)))
		SYS_LOG(LOG_ERROR,"error on [%s:%d]:%s\n",__FILE__,__LINE__,error);
	return cast(int32_t,ret);
}


void lua_timer_new(lua_State *L,wheelmgr *m,uint32_t timeout,luaRef *cb)
{
	timer *t     = cast(timer*,lua_newuserdata(L, sizeof(*t)));
	uint64_t now = accurate_tick64();
	memset(t,0,sizeof(*t));
	t->timeout   = timeout;
	t->cb        = lua_timeout_callback;
	t->luacb     = *cb;
	t->ud = cast(void*,&t->luacb);	
	if(!m->ptrtick){
		m->ptrtick  = &m->lasttick;
		m->lasttick = now;
	}
	t->expire = CAL_EXPIRE(now,t->timeout);
	_reg(m,t,m->lasttick,NULL);
	luaL_getmetatable(L, TIMER_METATABLE);
	lua_setmetatable(L, -2);			
}


static int32_t lua_register_timer(lua_State *L)
{
	wheelmgr *w       = lua_towheelmgr(L,1);
	uint32_t  timeout = cast(uint32_t,lua_tointeger(L,2));
	luaRef    cb      = toluaRef(L,3);
	if(!w)
		lua_pushnil(L);
	else{
		timeout = timeout > MAX_TIMEOUT ? MAX_TIMEOUT : (timeout > 0 ? timeout : 1); 
		lua_timer_new(L,w,timeout,&cb);
	}
	return 1;
}


static int32_t lua_unregister_timer(lua_State *L)
{
	timer    *t = lua_totimer(L,1);
	if(t->status & RELEASING)
		return 0;
	t->status |= RELEASING;
	if(!(t->status & INCB)){
		dlist_remove(cast(dlistnode*,t));
		release_luaRef(&t->luacb);
	}
	return 0;
}


static int32_t lua_timer_gc(lua_State *L)
{
	timer *t = lua_totimer(L,1);
	t->status |= RELEASING;
	if(cast(dlistnode*,t)->next && !(t->status & INCB))
	{
		dlist_remove(cast(dlistnode*,t));
		release_luaRef(&t->luacb);	
	}
	return 0;
}

static int32_t lua_wheel_tick(lua_State *L)
{
	wheelmgr *w = lua_towheelmgr(L,1);
	wheelmgr_tick(w,accurate_tick64());
	return 0;
}

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0) 


void reg_luatimewheel(lua_State *L)
{
    luaL_Reg wheelmgr_mt[] = {
        {"__gc", lua_wheelmgr_gc},
        {NULL, NULL}
    };

    luaL_Reg timer_mt[] = {
        {"__gc", lua_timer_gc},
        {NULL, NULL}
    };    


    luaL_Reg wheelmgr_methods[] = {
        {"Register",    lua_register_timer},
        {"Tick",           lua_wheel_tick},
        {NULL,     NULL}
    };

    luaL_Reg timer_methods[] = {
        {"UnRegister",    lua_unregister_timer},
        {NULL,     NULL}
    };    

    luaL_newmetatable(L, WHEELMGR_METATABLE);
    luaL_setfuncs(L, wheelmgr_mt, 0);

    luaL_newlib(L, wheelmgr_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_newmetatable(L, TIMER_METATABLE);
    luaL_setfuncs(L, timer_mt, 0);

    luaL_newlib(L, timer_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);    

    SET_FUNCTION(L,"TimingWheel",lua_wheelmgr_new);

}


#endif
