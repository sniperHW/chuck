#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "val.h"

static int objc = 0;

typedef union { double u; void *s; Val_Integer i; long l; } L_Umaxalign;

typedef struct TVal_string{
	TValueHead;
	size_t len;
	unsigned int hashcode;
}TVal_string;

union{
	L_Umaxalign dummy;
	TVal_string str;
}UTString;

struct Node;

typedef struct TKey{
  struct Node  *next;      /* for chaining (offset for next node) */	
  int           tt;
  uint64_t	    hashcode;
  union{
  	Val_Integer i;
  	Val_Boolean b;
  	Val_Number  n;
  	char 	   *s;
  };
}TKey;


typedef struct Node {	
  TValue *i_val;
  TKey    i_key;
} Node;


typedef struct TVal_table{
	TValueHead;
	size_t  cap;
	size_t  nsize;
	Node  **node;
}TVal_table;


int TVal_type(TValue *v){
	return v->tt;
}

int TVal_isinteger(TValue *v){
	return TVal_type(v) == TVAL_INTEGER;
}

int TVal_isnumber(TValue *v){
	return TVal_type(v) == TVAL_NUMBER;
}

int TVal_isboolean(TValue *v){
	return TVal_type(v) == TVAL_BOOLEAN;
}

int TVal_isstring(TValue *v){
	return TVal_type(v) == TVAL_STRING;
}

int TVal_istable(TValue *v){
	return TVal_type(v) == TVAL_TABLE;
}

#define cast(T,TS) ((T)TS)

#define getaddrstr(ts)  (cast(char *, (ts)) + sizeof(UTString))

TValue    *TVal_New_integer(Val_Integer i){
	TValue *v  = malloc(sizeof(*v));
	v->tt      = TVAL_INTEGER;
	v->value.i = i;
	v->rcount  = 1;
	v->dctor   = free;
	objc++;
	return v;
}

TValue     *TVal_New_number(Val_Number n){
	TValue *v  = malloc(sizeof(*v));
	v->tt      = TVAL_NUMBER;
	v->value.n = n;
	v->rcount  = 1;
	v->dctor   = free;
	objc++;
	return v;
}

TValue     *TVal_New_boolean(int b){
	TValue *v  = malloc(sizeof(*v));
	v->tt      = TVAL_BOOLEAN;
	v->value.b = b;
	v->rcount  = 1;
	v->dctor   = free;
	objc++;
	return v;
}

TValue     *TVal_New_string(const char *str){
	return 	TVal_New_lstring(str,strlen(str));
}

TValue     *TVal_New_lstring(const char *str,size_t len){
	TVal_string *v   = malloc(sizeof(UTString) + len + 1);
	char   *ptr = getaddrstr(v);
	memcpy(ptr,str,len);	
	cast(TVal_string*,v)->len = len;
	v->rcount  = 1;
	ptr[len] = 0;
	v->dctor   = free;
	v->value.o = cast(void*,v);
	v->tt      = TVAL_STRING;
	objc++;	
	return cast(TValue*,v); 	
}



#define fetch_number(T,V)						\
({  T __ret;									\
	switch(TVal_type(V)){						\
		case TVAL_INTEGER:{						\
			__ret = V->value.i;					\
			break;								\
		}										\
		case TVAL_NUMBER:{						\
			__ret = V->value.n;					\
			break;								\
		}										\
		case TVAL_BOOLEAN:{						\
			__ret = V->value.b;					\
			break;								\
		}										\
		default:								\
			assert(0);							\
	};											\
	__ret;})

Val_Integer TVal_tointeger(TValue *v)
{
	return fetch_number(Val_Integer,v);
}

Val_Number  TVal_tonumber(TValue *v)
{
	return fetch_number(Val_Number,v);
}

int         TVal_toboolean(TValue *v)
{
	int ret = fetch_number(int,v);
	return ret > 0 ? 1:0;
}

const char *TVal_tostring(TValue *v)
{
	if(v->tt != TVAL_STRING)
		return NULL;
	return getaddrstr(cast(TVal_string*,v));
}

const char *TVal_tolstring(TValue *v,size_t *len)
{
	if(v->tt != TVAL_STRING)
		return NULL;
	if(len) *len = cast(TVal_string*,v)->len;
	return getaddrstr(cast(TVal_string*,v));
}


TValue *TVal_retain(TValue *v){
	if(v) ++v->rcount;
	return v;
}

void TVal_release(TValue *v){
	if(v && --v->rcount == 0){
		v->dctor(v);
		objc--;
	}
}

uint64_t burtle_hash(uint8_t *k,uint64_t length,uint64_t level);

#define MAX_HASH_BYTE_STR 64


static inline unsigned int hashint(Val_Integer i)
{
	return burtle_hash(cast(uint8_t*,&i),sizeof(i),0);
}

static inline unsigned int hashboolean(Val_Boolean b)
{
	return hashint(b);
}

static inline unsigned int hashstr(const char *str)
{
	size_t len = strlen(str);
	return burtle_hash(cast(uint8_t*,str),len > MAX_HASH_BYTE_STR ? MAX_HASH_BYTE_STR:len,2);
}

static inline unsigned int hashnum(Val_Number n)
{
	return burtle_hash(cast(uint8_t*,&n),sizeof(n),1);
}


#define INDEX(T,H) H%T->cap
#define NODE(T,I)  T->node[I]


int   Key_Equal(TKey *key1,TKey *key2)
{
	if(key1->tt == key2->tt && key1->hashcode == key2->hashcode){
		switch(key1->tt){
			case TVAL_INTEGER:
			case TVAL_BOOLEAN:
			case TVAL_NUMBER:
			{
				if(key1->n == key2->n)
					return 0;
				break;				
			}			
			case TVAL_STRING:{
				if(strcmp(key1->s,key2->s) == 0)
					return 0;
				break;
			}			
			default:
				break;
		}
	}
	return 1;
}

#define SET_KEY(K1,K2) 											\
do{																\
	if(&(K1) == &(K2)) break;									\
	if((K1).tt == TVAL_STRING)									\
		free(cast(void*,(K1).s));								\
	(K1) = (K2);												\
	if((K2).tt == TVAL_STRING)									\
	{															\
		size_t len = strlen((K2).s);							\
		(K1).s  = malloc(len+1);								\
		memcpy((K1).s,(K2).s,len+1);							\
	}															\
}while(0)

//clear all field except next
#define CLEAR_KEY(K1)											\
do{																\
	if((K1).tt == TVAL_NONE) break;								\
	if((K1).tt == TVAL_STRING)									\
		free((K1).s);											\
	(K1) = (TKey){.next = (K1).next,							\
				  .tt=TVAL_NONE,								\
				  .i=0,											\
				  .hashcode = 0};								\
}while(0)

static inline void table_dcotr(void *_)
{
	TVal_table *t = cast(TVal_table*,_);
	size_t i;
	for(i=0;i < t->cap;++i){
		Node *n = NODE(t,i);
		if(n){
			CLEAR_KEY(n->i_key);
			TVal_release(n->i_val);
			free(n);
		}
	}
	free(t->node);
	free(t);
}

#define MAX_NODE_SIZE 1<<31

Node *New_Key(TVal_table *t,TKey *key)
{

#define EMPTYPLACE(T)									\
({ Node *n=NULL;size_t i;								\
   for(i=0;i < T->cap;++i)								\
   {													\
   	n = NODE(T,i);										\
   	if(!n){ n = calloc(1,sizeof(*n));					\
   			NODE(T,i) = n;								\
   			break;										\
   	} 													\
   	else if(!n->i_val) break;							\
   }													\
   n;})

	size_t idx = INDEX(t,key->hashcode);
	Node  *n   = NODE(t,idx);
	if(!n){
		n = calloc(1,sizeof(*n));
		SET_KEY(n->i_key,*key);
		n->i_val   	 = NULL;
		NODE(t,idx)  = n;
	}else if(n->i_key.tt != TVAL_NONE && 0 != Key_Equal(&n->i_key,key)){
		//colliding		
		size_t mainpos = INDEX(t,n->i_key.hashcode);
		Node *othern  = EMPTYPLACE(t);
		assert(othern);
		SET_KEY(othern->i_key,n->i_key);
		othern->i_val = n->i_val;
		n->i_val      = NULL; 
		SET_KEY(n->i_key,*key);			
		if(idx == mainpos)//othern in mainpos
			n->i_key.next = othern;
		else{//othern not in mainpos
			Node *m = NODE(t,mainpos);
			while(m->i_key.next != n)
				m = m->i_key.next;
			m->i_key.next = othern;
		}
	}
	return n;
#undef EMPTYPLACE	
}

Node *Table_Find(TVal_table *t,TKey *key)
{
	size_t idx = INDEX(t,key->hashcode);
	Node  *n   = NODE(t,idx);
	while(n && 0 != Key_Equal(&n->i_key,key)){
		n = n->i_key.next;
	}	
	return n;
}

static int table_resize(TVal_table *t,size_t newcap)
{
#define EMPTYPLACE(T)							\
({ size_t i;									\
   for(i=0;i < T->cap;++i)						\
   		if(!NODE(T,i))break; 					\
   i;})
	
	if(newcap > MAX_NODE_SIZE) return -1;
	size_t oldcap = t->cap;
	if(oldcap > newcap)
		printf("shrink\n");
	else
		printf("expand\n");
	Node **old = t->node;
	t->node    = calloc(1,sizeof(*t->node)*newcap);
	t->nsize   = 0;
	t->cap     = newcap;
	size_t i;
	for(i=0;i < oldcap;++i){	
		Node *n = old[i];
		if(n){
			if(n->i_key.tt == TVAL_NONE){
				free(n);
			}else{
				n->i_key.next = NULL;
				size_t idx = n->i_key.hashcode%newcap;
				if(!NODE(t,idx)){
					NODE(t,idx) = n;
				}
				else{
					//colliding
					Node *othern = NODE(t,idx);
					size_t mainpos = othern->i_key.hashcode%newcap;
					size_t empty = EMPTYPLACE(t);
					NODE(t,empty) = othern;
					NODE(t,idx) = n;	
					if(idx == mainpos)
						n->i_key.next  = othern;
					else{
						Node *m = NODE(t,mainpos);
						while(m->i_key.next != othern)
							m = m->i_key.next;
						m->i_key.next = othern;
					}					
				}
				++t->nsize;				
			}
		}
	}
	free(old);	
	return 0;
#undef EMPTYPLACE	
}


void TVal_Set(TVal_table *t,TKey *key,TValue *val)
{
	Node *n = Table_Find(t,key);
	if(n){
		if(!val){
			printf("remove:%ld\n",n->i_val->value.i);
			size_t mainpos = n->i_key.hashcode%t->cap;
			Node *m = NODE(t,mainpos);
			if(m != n){
				while(m->i_key.next != n)
					m = m->i_key.next;
				m->i_key.next = n->i_key.next;
			}
			CLEAR_KEY(n->i_key);
			TVal_release(n->i_val);
			n->i_val = NULL;
			if(--t->nsize == (t->cap >> 1) && t->cap > 16)
				table_resize(t,t->cap >> 1);//shrink			
		}else{
			TVal_release(n->i_val);
			n->i_val = TVal_retain(val);			
		}
	}else if(val){
		if(t->nsize == t->cap && t->nsize <= (MAX_NODE_SIZE >> 1))
		{
			if(0 != table_resize(t,t->cap << 1))//expand
				return;
		}
		Node *n = New_Key(cast(TVal_table*,t),key);
		n->i_val = TVal_retain(val);
		++t->nsize;		
	}
}

#define TABLE_GET(TYPE,KEY,HASH)												\
assert(t->tt == TVAL_TABLE);													\
if(t->tt != TVAL_TABLE) return NULL;											\
TKey k  = {.next=NULL,.tt=TYPE,KEY,.hashcode = HASH(key)};						\
Node *n = Table_Find(cast(TVal_table*,t),&k);									\
if(n){																			\
	return TVal_retain(n->i_val);												\
}																				\
return NULL

#define TABLE_SET(TYPE,KEY,HASH)												\
assert(t->tt == TVAL_TABLE);													\
if(t->tt != TVAL_TABLE) return;													\
TKey k  = {.next=NULL,.tt=TYPE,KEY,.hashcode = HASH(key)};						\
TVal_Set(cast(TVal_table*,t),&k,val)

void TVal_table_seti(TValue *t,Val_Integer key,TValue *val)
{
	TABLE_SET(TVAL_INTEGER,.i=key,hashint);
}
				

TValue *TVal_table_geti(TValue *t,Val_Integer key)
{
	TABLE_GET(TVAL_INTEGER,.i=key,hashint);
}

void TVal_table_setb(TValue *t,Val_Boolean key,TValue *val)
{
	TABLE_SET(TVAL_INTEGER,.b=key,hashboolean);
}

TValue *TVal_table_getb(TValue *t,Val_Boolean  key)
{
	TABLE_GET(TVAL_INTEGER,.b=key,hashboolean);	
}

void TVal_table_setn(TValue *t,Val_Number key,TValue *val)
{
	TABLE_SET(TVAL_NUMBER,.n=key,hashnum);
}

TValue     *TVal_table_getn(TValue *t,Val_Number key)
{
	TABLE_GET(TVAL_NUMBER,.n=key,hashnum);	
}

void TVal_table_sets(TValue *t,const char *key,TValue *val)
{
	TABLE_SET(TVAL_STRING,.s=cast(char*,key),hashstr);	
}

TValue     *TVal_table_gets(TValue *t,const char *key)
{
	TABLE_GET(TVAL_STRING,.s=cast(char*,key),hashstr);
}


#define INIT_SIZE 16

TValue     *TVal_New_table()
{
	TVal_table *table = malloc(sizeof(*table));
	table->rcount  = 1;
	table->cap     = INIT_SIZE;
	table->nsize   = 0;
	table->node    = calloc(INIT_SIZE,sizeof(*table->node));
	table->dctor   = table_dcotr;
	table->value.o = cast(void*,table);
	table->tt      = TVAL_TABLE;
	objc++;	
	return cast(TValue*,table); 	
}


static iter  TVal_table_next(TValue *_,iter *it)
{
	TVal_table *t = cast(TVal_table*,_);	
	iter ret;
	size_t i = -1;
	Node *n;
	if(!(it == NULL || it == &nil_iter)){
		i = it->i;
		TVal_release(it->value);
		TVal_release(it->key);
	}

	do{
		if(++i >= t->cap)
			return nil_iter;
		n = t->node[i];
		if(n && n->i_val)
			break;
	}while(1);	
	
	if(n && n->i_val){
		switch(n->i_key.tt){
			case TVAL_INTEGER:{
				ret.key = TVal_New_integer(n->i_key.i);
				break;
			}
			case TVAL_NUMBER:{
				ret.key = TVal_New_number(n->i_key.n);
				break;
			}
			case TVAL_BOOLEAN:{
				ret.key = TVal_New_boolean(n->i_key.b);
				break;
			}
			case TVAL_STRING:{
				ret.key = TVal_New_string(n->i_key.s);
				break;
			}
			default: return nil_iter;
		}		
		ret.value = TVal_retain(n->i_val);
		ret.i     = i;
	}else{
		return nil_iter;
	}
	return ret;	
}


iter TVal_next(TValue *t,iter *it)
{
	if(t->tt == TVAL_TABLE)
		return TVal_table_next(t,it);
	return nil_iter;
}


//lua interface

static TValue *toTable(lua_State *L,int idx)
{
	TValue *table = TVal_New_table();
	int oldtop = lua_gettop(L);
	lua_pushnil(L);
	do{		
		if(!lua_next(L,idx)){
			break;
		}
		int key_type = lua_type(L, -2);
		int val_type = lua_type(L, -1);

		TValue *v = NULL;
		switch(val_type){
			case LUA_TNUMBER:{
				Val_Number n = lua_tonumber(L,-1); 
				if(n != (lua_Integer)n)
					v = TVal_New_number(n);
				else
					v = TVal_New_integer(lua_tointeger(L,-1));
				break;
			}
			case LUA_TSTRING:{
				size_t len;
				const char *str = lua_tolstring(L,-1,&len);
				v = TVal_New_lstring(str,len);
				break;
			}
			case LUA_TBOOLEAN:{
				v = TVal_New_boolean((int)lua_tointeger(L,-1));
				break;
			}
			case LUA_TTABLE:{
				v = toTable(L,-1);
				break;
			}
			default:break;
		}
		if(v){
			switch(key_type){
				case LUA_TNUMBER:{
					Val_Number n = lua_tonumber(L,-2); 
					if(n != (lua_Integer)n)
						TVal_table_setn(table,n,v);
					else
						TVal_table_seti(table,(Val_Integer)n,v);
					break;
				}
				case LUA_TSTRING:{
					const char *str = lua_tostring(L,-2);
					TVal_table_sets(table,str,v);
					break;
				}
				case LUA_TBOOLEAN:{
					TVal_table_setb(table,(int)lua_tointeger(L,-2),v);
					break;
				}
				default:break;
			}
			TVal_release(v);			
		}
		lua_pop(L,1);
	}while(1);
	lua_settop(L,oldtop);
	return table;
}

TValue *toVal(lua_State *L,int idx)
{
	TValue *v = NULL;
	int type = lua_type(L,idx);
	switch(type){
		case LUA_TNUMBER:{
			Val_Number n = lua_tonumber(L,idx); 
			if(n != (lua_Integer)n)
				v = TVal_New_number(n);
			else
				v = TVal_New_integer(lua_tointeger(L,idx));
			break;
		}
		case LUA_TSTRING:{
			size_t len;
			const char *str = lua_tolstring(L,idx,&len);
			v = TVal_New_lstring(str,len);
			break;
		}
		case LUA_TBOOLEAN:{
			v = TVal_New_boolean((int)lua_tointeger(L,idx));
			break;
		}
		case LUA_TTABLE:{
			v = toTable(L,idx);
			break;
		}
		default:break;
	}
	return v;
}

static void pushTable(lua_State *L,TValue *v)
{
	lua_newtable(L);
	iter it = TVal_next(v,NULL);
	while(TVal_is_vaild_iter(&it)){
		if(0 == pushVal(L,it.key)){
			pushVal(L,it.value);
			lua_rawset(L,-3);
		}else{
			//pop the nil
			lua_pop(L,1);
		}
		it = TVal_next(v,&it);
	}	
}

int pushVal(lua_State *L,TValue *v)
{
	int type = TVal_type(v);
	switch(type){
		case TVAL_INTEGER:{
			lua_pushinteger(L,TVal_tointeger(v));
			break;
		}
		case TVAL_BOOLEAN:{
			lua_pushinteger(L,TVal_toboolean(v));
			break;
		}
		case TVAL_NUMBER:{
			lua_pushinteger(L,TVal_tonumber(v));
			break;
		}
		case TVAL_STRING:{
			size_t len;
			const char *str = TVal_tolstring(v,&len);
			lua_pushlstring(L,str,len);
			break;
		}
		case TVAL_TABLE:{
			pushTable(L,v);
			break;
		}
		default:{
			lua_pushnil(L);
			return -1;
			break;
		}
	}
	return 0;
}

void TVal_table_show_list(TValue *v)
{
	if(v->tt == TVAL_TABLE){
		TVal_table *t = cast(TVal_table*,v);
		size_t i = 0;
		for(;i < t->nsize;++i){
			Node *n = t->node[i];
			printf("list:\n");
			while(n){
				if(n->i_val){
					TVal_print(n->i_val);printf(",");
				}
				n = n->i_key.next;
			}
			printf("\n");
		}
	}
}

void TVal_print(TValue *v){
	switch(TVal_type(v)){
		case TVAL_INTEGER:{
			printf("%ld",TVal_tointeger(v));
			break;
		}
		case TVAL_NUMBER:{
			printf("%f",TVal_tonumber(v));
			break;
		}		
		case TVAL_BOOLEAN:{
			printf("%s",TVal_toboolean(v) ? "true":"false");
			break;
		}
		case TVAL_STRING:{
			printf("%s",TVal_tostring(v));
			break;
		}
		case TVAL_TABLE:{
			printf("table:%lx",cast(Val_Integer,v));
			break;
		}	
		default:{
			printf("unknow TValue type");
			break;
		}
	}
}

int         TVal_objc(){
	return objc;
}