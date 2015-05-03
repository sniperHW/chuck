#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "string.h"
#include "refobj.h"
#include "comm.h"

typedef struct
{
	refobj     ref;
	char*      str;
	uint32_t   len;
	uint32_t   cap;
}holder;


static void holder_dctor(void *arg)
{
	holder *h = (holder*)arg;
	free((void*)h->str);
	free(h);
}

static inline holder *holder_new(const char *str,uint32_t len)
{
	holder *h = calloc(1,sizeof(*h));
	h->cap = size_of_pow2(len+1);
	h->str = calloc(1,h->cap);
	strncpy(h->str,str,len);
	h->str[len] = 0;
	h->len = len;
	refobj_init(&h->ref,holder_dctor);
	return h;
}

static inline void holder_append(holder *h,const char *str,uint32_t len){
	uint32_t total_len = h->len + len +1;
	if(h->cap < total_len){
		h->cap = size_of_pow2(total_len);
		char *tmp = realloc(h->str,h->cap);
		if(!tmp){
			tmp = calloc(1,h->cap);
			strcpy(tmp,h->str);
			free(h->str);
		}
		h->str = tmp;
	}
	strcat(h->str,str);
	h->len = total_len - 1;
}


static inline void holder_release(holder *h)
{
	if(h) refobj_dec(&h->ref);
}

static inline void holder_acquire(holder *h)
{
    if(h) refobj_inc(&h->ref);
}


#define MIN_STRING_LEN 64

typedef struct string
{
	holder  *holder;
	uint32_t len;
	char     str[MIN_STRING_LEN];
}string;


string *string_new(const char *str)
{
	if(!str) return NULL;
	string *_str = calloc(1,sizeof(*_str));
	uint32_t len = strlen(str);
	if(len + 1 <= MIN_STRING_LEN){
		strcpy(_str->str,str);
		_str->len = len;
	}
	else
		_str->holder = holder_new(str,len);
	return _str;
}

string* string_copy_new(string *oth)
{
	string *str = calloc(1,sizeof(*str));
	if(!oth->holder){
		strncpy(str->str,oth->str,oth->len);
		str->len = oth->len;
	}else{
		holder_acquire(oth->holder);		
		str->holder = oth->holder;
	}
	return str;
}

void string_del(string *s)
{
	if(s->holder){
		holder_release(s->holder);
	}
	free(s);
}

const char *string_cstr(string *s){
	if(s->holder){
		return s->holder->str;
	}
	return s->str;
}

int32_t  string_len(string *s)
{
	if(s->holder){
		return s->holder->len;
	}
	return s->len;
}


void string_append(string *s,const char *str)
{
	if(!s || !str) return;
	uint32_t len = strlen(str);
	uint32_t total_len = string_len(s) + len + 1;
	if(s->holder)
		holder_append(s->holder,str,len);	
	else if(total_len <= MIN_STRING_LEN){
		strcat(s->str,str);
		s->len = total_len - 1;
	}
	else{
		s->holder = holder_new(string_cstr(s),s->len);
		holder_append(s->holder,str,len);
	}
}
