/*
    Copyright (C) <2015>  <sniperHW@163.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LUA_UTIL_H
#define _LUA_UTIL_H

#ifdef _MYLUAJIT

#include <LuaJIT-2.0.4/src/lua.h>  
#include <LuaJIT-2.0.4/src/lauxlib.h>  
#include <LuaJIT-2.0.4/src/lualib.h>

static inline void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup){
	if(!l) return;
	if(!lua_istable(L,-1)) return;
	while(l->name){
		lua_pushstring(L,l->name);
		lua_pushcfunction(L, l->func);
		lua_rawset(L, -3);
		++l;
	}
}

#define luaL_newlibtable(L,l)	\
  lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)  \
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

#else

#include <lua.h>  
#include <lauxlib.h>  
#include <lualib.h>

#endif

#include <stdio.h>
#include <stdlib.h>


typedef struct{
	lua_State     *L;
	int 		   rindex;	
}luaRef;

typedef struct luaPushFunctor{
	void (*Push)(lua_State *L,struct luaPushFunctor *self);
}luaPushFunctor;

static inline void release_luaRef(luaRef *ref)
{
	if(ref->L && ref->rindex != LUA_REFNIL){
		luaL_unref(ref->L,LUA_REGISTRYINDEX,ref->rindex);
		ref->L = NULL;
		ref->rindex = LUA_REFNIL;
	}
}

static inline luaRef toluaRef(lua_State *L,int idx)
{
	luaRef ref;
	lua_pushvalue(L,idx);
	ref.rindex = luaL_ref(L,LUA_REGISTRYINDEX);
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

static inline void push_LuaRef(lua_State *L,luaRef ref)
{
	lua_rawgeti(L,LUA_REGISTRYINDEX,ref.rindex);
}

/* 当ref是一个table时，可通过调用以下两个函数获取和设置table字段
*  fmt "kvkvkv",k,v含义与luacall一致,且数量必须配对
*  Get的时候v是获取的类型
*  Set的时候v是写入的类型
*/
const char *LuaRef_Get(luaRef ref,const char *fmt,...);
const char *LuaRef_Set(luaRef ref,const char *fmt,...);

/*i:符号整数
* u:无符号整数
* s:字符串,lua_pushstring,lua_tostring
* S:字符串,lua_pushlstring,lua_tolstring,注:S后跟的长度字段必须为size_t/size_t*
* b:布尔值,必须为int/int*
* n:lua number
* r:lua ref
* p:指针(lightuserdata)
*/
const char *luacall(lua_State *L,const char *fmt,...);

#define LuaCall(__L,__FUNC,__FMT, ...)({\
			const char *__result;\
			int __oldtop = lua_gettop(__L);\
			lua_getglobal(__L,__FUNC);\
			__result = luacall(__L,__FMT,##__VA_ARGS__);\
			lua_settop(__L,__oldtop);\
		__result;})

//调用一个lua引用，这个引用是一个函数		
#define LuaCallRefFunc(__FUNREF,__FMT,...)({\
			const char *__result;\
			lua_State *__L = (__FUNREF).L;\
			int __oldtop = lua_gettop(__L);\
			lua_rawgeti(__L,LUA_REGISTRYINDEX,(__FUNREF).rindex);\
			__result = luacall(__L,__FMT,##__VA_ARGS__);\
			lua_settop(__L,__oldtop);\
		__result;})

//调用luatable的一个函数字段,注意此调用方式相当于o:func(),也就是会传递self		
#define LuaCallTabFuncS(__TABREF,__FIELD,__FMT,...)({\
			const char *__result;\
			lua_State *__L = (__TABREF).L;\
			int __oldtop = lua_gettop(__L);\
			lua_rawgeti(__L,LUA_REGISTRYINDEX,(__TABREF).rindex);\
			lua_pushstring(__L,__FIELD);\
			lua_gettable(__L,-2);\
			lua_remove(__L,-2);\
			const char *__fmt = __FMT;\
			if(__fmt){char __tmp[32];\
				snprintf(__tmp,32,"r%s",(const char*)__fmt);\
				__result = luacall(__L,__tmp,__TABREF,##__VA_ARGS__);\
			}else{\
				__result = luacall(__L,"r",__TABREF);\
			}\
			lua_settop(__L,__oldtop);\
		__result;})
		
//调用luatable的一个函数字段,注意此调用方式相当于o.func(),也就是不传递self		
#define LuaCallTabFunc(__TABREF,__FIELD,__FMT,...)({\
			const char *__result;\
			lua_State *__L = (__TABREF).L;\
			int __oldtop = lua_gettop(__L);\
			lua_rawgeti(__L,LUA_REGISTRYINDEX,(__TABREF).rindex);\
			lua_pushstring(__L,__FIELD);\
			lua_gettable(__L,-2);\
			lua_remove(__L,-2);\
			__result = luacall(__L,__FMT,##__VA_ARGS__);\
			lua_settop(__L,__oldtop);\
		__result;})	


#define EnumKey -2
#define EnumVal -1				
#define LuaTabForEach(TABREF)\
			for(lua_rawgeti((TABREF).L,LUA_REGISTRYINDEX,(TABREF).rindex),lua_pushnil((TABREF).L);\
				({\
					int __result;\
					do __result = lua_next((TABREF).L,-2);\
					while(0);\
					if(!__result)lua_pop((TABREF).L,1);\
					__result;});lua_pop((TABREF).L,1))

//make a lua object by str and return the reference
luaRef makeLuaObjByStr(lua_State *L,const char *);


#endif


