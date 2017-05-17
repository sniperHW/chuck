#ifndef _CHK_UD_H
#define _CHK_UD_H

#include <stdint.h>

#if CHUCK_LUA
#include "lua/chk_lua.h"
#endif


typedef struct{
	union {
		void 		*val;
		int64_t  	 i64;
		uint64_t 	 u64;
#if CHUCK_LUA
		chk_luaRef   lr;
#endif
	}v;
}chk_ud;

static inline chk_ud chk_ud_make_void(void *v) {
	chk_ud ud;
	ud.v.val = v;
	return ud;
}

static inline chk_ud chk_ud_make_i64(int64_t v) {
	chk_ud ud;
	ud.v.i64 = v;
	return ud;	
}

static inline chk_ud chk_ud_make_u64(uint64_t v) {
	chk_ud ud;
	ud.v.u64 = v;
	return ud;	
}

#if CHUCK_LUA

static inline chk_ud chk_ud_make_lr(chk_luaRef v) {
	chk_ud ud;
	ud.v.lr = v;
	return ud;	
}

#endif


#endif