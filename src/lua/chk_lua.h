#ifndef _CHK_LUA_H
#define _CHK_LUA_H

#ifdef _MYLUAJIT

#include <LuaJIT-2.0.4/src/lua.h>  
#include <LuaJIT-2.0.4/src/lauxlib.h>  
#include <LuaJIT-2.0.4/src/lualib.h>

#else

#include <lua.h>  
#include <lauxlib.h>  
#include <lualib.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM==501
/*
** Adapted from Lua 5.2.0
*/
static inline void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
	luaL_checkstack(L, nup+1, "too many upvalues");
	for (; l->name != NULL; l++) {	/* fill the table with given functions */
		int i;
		lua_pushstring(L, l->name);
		for (i = 0; i < nup; i++)	/* copy upvalues to the top */
			lua_pushvalue(L, -(nup + 1));
		lua_pushcclosure(L, l->func, nup);	/* closure with those upvalues */
		lua_settable(L, -(nup + 3));
	}
	lua_pop(L, nup);	/* remove upvalues */
}

#define luaL_newlibtable(L,l)												\
  lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)  													\
  (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

static inline void *luaL_testudata (lua_State *L, int ud, const char *tname) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
      luaL_getmetatable(L, tname);  /* get correct metatable */
      if (!lua_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      lua_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}

#endif

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


static inline void chk_push_LuaRef(lua_State *L,chk_luaRef ref);
static inline chk_luaRef chk_toluaRef(lua_State *L,int idx);

/*
static inline chk_luaRef chk_luaRef_retain(chk_luaRef *ref) {
	if(ref->L && ref->index != LUA_REFNIL) {
		chk_push_LuaRef(ref->L,ref);
		return chk_toluaRef(ref->L,-1);
	}
	return (chk_luaRef){.L=NULL};
}
*/

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

#ifdef _MYLUAJIT
	lua_pushmainthread(L);
	ref.L = lua_tothread(L,-1);
#else	
	lua_rawgeti(L,  LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	ref.L = lua_tothread(L,-1);
#endif
	lua_pop(L,1);	
	return ref;
}

static inline void chk_push_LuaRef(lua_State *L,chk_luaRef ref) {
	assert(L && (ref.index != LUA_REFNIL && ref.index != LUA_REFNIL));
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


