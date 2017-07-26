#ifndef _CHK_LUA_H
#define _CHK_LUA_H

#include <lua.h>  
#include <lauxlib.h>  
#include <lualib.h>
#include <stdlib.h>

typedef struct chk_luaRef chk_luaRef;

typedef struct chk_luaPushFunctor chk_luaPushFunctor;

typedef struct chk_luaToFunctor   chk_luaToFunctor;

struct chk_luaRef {
	lua_State *L;
	int 	   index;	
};

//用于向lua栈push用户自定义类型
struct chk_luaPushFunctor {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
};

//用于从lua栈获取用户自定义类型
struct chk_luaToFunctor {
	void (*To)(chk_luaToFunctor *self,lua_State *L,int idx);
};

static inline void chk_luaRef_release(chk_luaRef *ref) {
	if(ref->L) {
		luaL_unref(ref->L,LUA_REGISTRYINDEX,ref->index);
		ref->L = NULL;
	}
}

static inline chk_luaRef chk_toluaRef(lua_State *L,int idx) {
	chk_luaRef ref = {.L = NULL};
	lua_pushvalue(L,idx);
	ref.index = luaL_ref(L,LUA_REGISTRYINDEX);
	if(ref.index == LUA_REFNIL || ref.index == LUA_NOREF)
		return ref;	

	lua_rawgeti(L,  LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	ref.L = lua_tothread(L,-1);
	lua_pop(L,1);	
	return ref;
}

static inline void chk_push_LuaRef(lua_State *L,chk_luaRef ref) {
	lua_rawgeti(L,LUA_REGISTRYINDEX,ref.index);
}

/*i:符号整数
* u:无符号整数
* s:字符串,lua_pushstring,lua_tostring
* S:字符串,lua_pushlstring,lua_tolstring,注:S后跟的长度字段必须为size_t/size_t*
* b:布尔值,必须为int/int*
* n:lua number
* r:lua ref
* p:指针(lightuserdata)
* f:chk_luaPushFunctor
* t:chk_luaToFunctor,只能与返回参数
*/
const char *chk_lua_pcall(lua_State *L,const char *fmt,...);

#define chk_Lua_PCall(__L,__FUNC,__FMT, ...)			  		 ({\
	const char *__result;										   \
	int __oldtop = lua_gettop(__L);							       \
	lua_getglobal(__L,__FUNC);								       \
	__result = chk_lua_pcall(__L,__FMT,##__VA_ARGS__);		       \
	lua_settop(__L,__oldtop);								       \
    __result;												     })

//调用一个lua函数引用		
#define chk_Lua_PCallRef(__FUNREF,__FMT,...)			         ({\
	const char *__result = "invaild ref";						   \
	lua_State *__L = (__FUNREF).L;								   \
	if(__L) {							       					   \
		int __oldtop = lua_gettop(__L);							   \
		lua_rawgeti(__L,LUA_REGISTRYINDEX,(__FUNREF).index);	   \
		__result = chk_lua_pcall(__L,__FMT,##__VA_ARGS__);		   \
		lua_settop(__L,__oldtop);								   \
	}								       						   \
	__result;												     })

/*
#include "util/chk_obj_pool.h"

DECLARE_OBJPOOL(chk_luaRef)

extern chk_luaRef_pool *luaRef_pool;

extern int32_t lock_luaRef_pool;

#define LUAREF_POOL_LOCK(L) while (__sync_lock_test_and_set(&lock_luaRef_pool,1)) {}
#define LUAREF_POOL_UNLOCK(L) __sync_lock_release(&lock_luaRef_pool);

#ifndef INIT_LUAREF_POOL_SIZE
#define INIT_LUAREF_POOL_SIZE 4096
#endif

#define POOL_NEW_LUAREF()         ({                                 \
    chk_luaRef *ref;                                                 \
    LUAREF_POOL_LOCK();                                              \
    if(NULL == luaRef_pool) {                                        \
        luaRef_pool = chk_luaRef_pool_new(INIT_LUAREF_POOL_SIZE);    \
    }                                                                \
    ref = chk_luaRef_new_obj(luaRef_pool);                       	 \
    LUAREF_POOL_UNLOCK();                                            \
    ref;                                                             \
})

#define POOL_RELEASE_LUAREF(LUA_REF) do{                             \
	chk_luaRef_release(LUA_REF);									 \
    LUAREF_POOL_LOCK();                                              \
    chk_luaRef_release_obj(luaRef_pool,LUA_REF);                     \
    LUAREF_POOL_UNLOCK();                                            \
}while(0)
*/

#if 0

/* 当ref是一个table时，可通过调用以下两个函数获取和设置table字段
*  fmt "kvkvkv",k,v含义与luacall一致,且数量必须配对
*  Get的时候v是获取的类型
*  Set的时候v是写入的类型
*/
const char *LuaRef_Get(luaRef ref,const char *fmt,...);
const char *LuaRef_Set(luaRef ref,const char *fmt,...);

#define EnumKey -2
#define EnumVal -1				
#define chk_LuaTableLoop(TABREF)								   \
	for(lua_rawgeti((TABREF).L,LUA_REGISTRYINDEX,(TABREF).index),  \
		lua_pushnil((TABREF).L);								   \
		({int __result = lua_next((TABREF).L,-2);				   \
		  if(!__result)lua_pop((TABREF).L,1);					   \
		__result;});lua_pop((TABREF).L,1))
#endif

#endif


