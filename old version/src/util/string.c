#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "util/string.h"
#include "util/refobj.h"

#define MIN_STRING_LEN 64

struct string
{	
	REFOBJ;
	char    *ptr;
	size_t   cap;
	size_t   len;
	char     short_str[MIN_STRING_LEN];
};

static inline void dctor(void *_)
{
	string *s = cast(string*,_);
	if(s->ptr != s->short_str)
		free(s->ptr);
	free(s);
}	

string *string_new(const char *str,size_t len)
{
	string *s = calloc(1,sizeof(*s));
	if(len < MIN_STRING_LEN){
		s->ptr = s->short_str;
		s->cap = MIN_STRING_LEN;
	}else{
		s->cap = size_of_pow2(len+1);
		s->ptr = calloc(1,s->cap);
	}
	s->len = len;
	memcpy(s->ptr,str,len);
	refobj_init(cast(refobj*,s),dctor);
	return s;
}


string *string_retain(string *s)
{
	if(s) refobj_inc(cast(refobj*,s));
	return s;
}

void    string_release(string *s)
{
	if(s) refobj_dec(cast(refobj*,s));
}


const char *string_cstr(string *s)
{
	return s->ptr;
}

size_t string_len(string *s)
{
	return s->len;
}

void   string_append(string *s,const char *str,size_t len)
{
	char  *tmp;
	size_t space_need = s->len + len + 1;
	if(s->cap < space_need){
		s->cap = size_of_pow2(space_need);
		if(s->ptr != s->short_str){
			tmp = realloc(s->ptr,s->cap);
			if(!tmp){
				tmp = calloc(1,s->cap);
				memcpy(tmp,s->ptr,s->len);
				free(s->ptr);
			}
		}else{
			tmp = calloc(1,s->cap);
			memcpy(tmp,s->ptr,s->len);
		}
		s->ptr = tmp;
	}
	memcpy(s->ptr + s->len,str,len);
	s->len += len;	
}


#ifdef _CHUCKLUA

void   push_string(lua_State *L,string *s){
	if(!s || s->len <= 0){
		lua_pushnil(L);
	}else{
		lua_pushlstring(L,s->ptr,s->len);
	}
}

#endif

