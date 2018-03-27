
typedef struct sche sche;

typedef struct coroutine {
	lua_State *L;
	struct coroutine *prev;
	struct coroutine *next;
	int    coIndex;     //用于持有thread的索引，防止被GC
	int    selfIndex;	
	int    resume_argcount;
}coroutine;

typedef struct sche{
	lua_State *L;
	coroutine  readyHead;
	coroutine  readyTail;
	coroutine *running;   //当前正在运行的coroutine
}sche;


#define COROUTINE_META      "coroutine_mt"
#define lua_check_coroutine(L,I) (coroutine*)luaL_checkudata(L,I,COROUTINE_META)

const char *index_g_sche = "chuck.g_sche";

static inline sche *get_sche(lua_State *L) {
	sche *g_sche = NULL;
	lua_State *mL = NULL;		
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	mL = lua_tothread(L,-1);
	lua_pop(L,1);
	lua_pushstring(mL,index_g_sche);
	lua_rawget(mL,LUA_REGISTRYINDEX);
	g_sche = lua_touserdata(mL,-1);
	lua_pop(mL,1);
	return g_sche;
}

static void push_ready(sche *g_sche,coroutine *co) {	
	coroutine *prev = g_sche->readyTail.prev;
	co->prev = prev;
	prev->next = co;
	co->next = &g_sche->readyTail;
	g_sche->readyTail.prev = co;
}

static int coroutine_new(lua_State *L) {

	if(!lua_isfunction(L,1)) {
		return luaL_error(L,"argument must be a lua function");
	}

	coroutine *co = LUA_NEWUSERDATA(L,coroutine);

	if(!co) {
		return luaL_error(L,"calloc failed");
	} else {

		luaL_setmetatable(L, COROUTINE_META);

		sche *g_sche = get_sche(L);

		lua_State *NL = lua_newthread(L);
		int index = luaL_ref(L,LUA_REGISTRYINDEX);
	  	lua_pushvalue(L, 1); /* move function to top */
	  	lua_xmove(L, NL, 1); /* move function from L to NL */
	  	
		lua_pushvalue(L, -1); /* move userdata to top */
	  	lua_xmove(L, NL, 1); /* move userdata from L to NL */	

		int top = lua_gettop(L) - 1;
		int i;
		for( i = 2; i <= top; ++i) {
			lua_pushvalue(L,i);
			lua_xmove(L, NL, 1);
		}

		co->L = NL;
		co->coIndex = index;
		co->resume_argcount = top - 1 + 1;
		push_ready(g_sche,co);
		


		lua_pushvalue(L, -1);
		if(L != g_sche->L) {
			lua_xmove(L, g_sche->L, 1);
		}
		co->selfIndex = luaL_ref(g_sche->L,LUA_REGISTRYINDEX);
		return 1;
	}
}

static int coroutine_yield_a_while(lua_State *L) {
	sche *g_sche = get_sche(L);
	if(g_sche->running) {
		push_ready(g_sche,g_sche->running);
		lua_yield(L,0);
	} else {
		luaL_error(L,"yield must call under coroutine");
	}
	return 0;	
}

static int coroutine_yield(lua_State *L) {
	sche *g_sche = get_sche(L);
	if(g_sche->running) {
		lua_yield(L,0);
	} else {
		return luaL_error(L,"yield must call under coroutine");
	}
	return 0;
}

static int coroutine_running(lua_State *L) {
	sche *g_sche = get_sche(L);	
	if(g_sche->running && g_sche->running->selfIndex != LUA_NOREF) {
		lua_rawgeti(g_sche->L,LUA_REGISTRYINDEX,g_sche->running->selfIndex);
		lua_xmove(g_sche->L, L, 1);		
		return 1;
	} else {
		return 0;
	}
}

static int resume_coroutine(sche *g_sche,coroutine *co) {
	g_sche->running = co;
	int ret = lua_resume(co->L,g_sche->L,co->resume_argcount);
	if(ret != LUA_YIELD) {
		//线程函数结束，coroutine可以被gc,释放对自身的引用
		luaL_unref(g_sche->L,LUA_REGISTRYINDEX,co->selfIndex);
		co->selfIndex = LUA_NOREF;
		if(ret != LUA_OK) {
			CHK_SYSLOG(LOG_ERROR,"resume_coroutine error:%s",lua_tostring(co->L,-1));
		}
	}
	return 0;
}

static int sche_loop(lua_State *L) {
	sche *g_sche = get_sche(L);	
	coroutine *co;
	while(&g_sche->readyTail != (co = g_sche->readyHead.next)){
		g_sche->readyHead.next = co->next;
		co->next->prev = &g_sche->readyHead;
		co->next = co->prev = NULL;
		resume_coroutine(g_sche,co);		
	}
	g_sche->running = NULL;
	return 0;
}

static int coroutine_resume(lua_State *L) {
	sche *g_sche = get_sche(L);	
	coroutine *co = lua_check_coroutine(L,1);
	if(g_sche->running == co) {
		return luaL_error(L,"coroutine is running");
	}else if(!(co->prev || co->next)) {
		int top = lua_gettop(L);
		int i;
		for( i = 2; i <= top; ++i) {
			lua_pushvalue(L,i);
			lua_xmove(L, co->L, 1);
		}	
		co->resume_argcount = top - 1;
		push_ready(g_sche,co);
	}

	if(!g_sche->running) {
		sche_loop(L);
	}
	return 0;
}

static int coroutine_resume_and_yield(lua_State *L) {
	sche *g_sche = get_sche(L);	
	coroutine_resume(L);
	if(g_sche->running) {
		lua_yield(L,0);
	}
	return 0;
}

static int coroutine_gc(lua_State *L) {
	sche *g_sche = get_sche(L);	
	//如果g_sche为NULL表示lua_State *L已经关闭所有对象都被gc,下面的代码也就无需执行了。
	coroutine *co = lua_check_coroutine(L,1);
	if(co->coIndex != LUA_NOREF) {
		luaL_unref(g_sche->L,LUA_REGISTRYINDEX,co->coIndex);
	}
	return 0;
}

static void register_coroutine(lua_State *L) {

	luaL_Reg coroutine_mt[] = {
		{"__gc",coroutine_gc},
		{NULL, NULL}
	};

	luaL_newmetatable(L, COROUTINE_META);
	luaL_setfuncs(L, coroutine_mt, 0);
	lua_pop(L, 1);

	lua_pushstring(L,index_g_sche);	
	sche *g_sche = LUA_NEWUSERDATA(L,sche);
	g_sche->L = L;
	g_sche->readyHead.next = &g_sche->readyTail;
	g_sche->readyTail.prev = &g_sche->readyHead;
	lua_rawset(L,LUA_REGISTRYINDEX);

	lua_newtable(L);
	SET_FUNCTION(L,"new",coroutine_new);
	SET_FUNCTION(L,"sche",sche_loop);
	SET_FUNCTION(L,"running",coroutine_running);
	SET_FUNCTION(L,"yield",coroutine_yield); 
	SET_FUNCTION(L,"yield_a_while",coroutine_yield_a_while);     
	SET_FUNCTION(L,"resume",coroutine_resume);
	SET_FUNCTION(L,"resume_and_yield",coroutine_resume_and_yield);
}