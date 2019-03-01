#include "util/chk_list.h"
#include <float.h>

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

#define WPACKET_METATABLE "lua_wpacket"

#define RPACKET_METATABLE "lua_rpacket"


typedef struct {
    chk_bytebuffer *buff;               
}lua_wpacket;


typedef struct {
	chk_bytebuffer *buff;
    chk_bytechunk  *cur;       
    uint32_t        readpos;           
    uint32_t        data_remain;
    uint32_t        total_packet_size;        
}lua_rpacket;


/*
*	_lua_pack_table时用于检测table是否存在循环引用,
*   如果出现循环引用将导致_lua_pack_table死循环
*   
*   循环引用的例子:
*	t1 = {}
*	t2 = {}
*	t1[1] = 1
*	t1[2] = t2
*	t2[1] = 2
*	t2[2] = t1
*   todo:优化检测算法
*/

typedef struct {
	chk_list_entry list_entry;
	const	void  *p;
}cycle_check_node;

__thread chk_list cycle_check_list = {0,NULL,NULL};

static void clean_cycle_check_list() {
	cycle_check_node *node;
	while(NULL != (node = (cycle_check_node*)chk_list_pop(&cycle_check_list))) {
		free(node);
	}
}

static void check_cycle(lua_State *L,int32_t index) {
	cycle_check_node *node;
	const void *p = lua_topointer(L,index);
	node = (cycle_check_node*)chk_list_begin(&cycle_check_list);
	while(NULL != node) {
		if(p == node->p) {
			clean_cycle_check_list();
			CHK_SYSLOG(LOG_ERROR,"check_cycle() table has cycle ref");	
			luaL_error(L,"check_cycle() table has cycle ref");
		}
		node = (cycle_check_node*)((chk_list_entry*)node)->next;
	}
	node = calloc(1,sizeof(*node));
	if(NULL == node) {
		clean_cycle_check_list();
		CHK_SYSLOG(LOG_ERROR,"check_cycle() calloc(node) failed");		
		luaL_error(L,"check_cycle() not enough memroy");	
	}
	node->p = p;
	chk_list_pushback(&cycle_check_list,(chk_list_entry*)node);
}

#define lua_checkbytebuffer(L,I)	\
	(chk_bytebuffer*)luaL_checkudata(L,I,BYTEBUFFER_METATABLE)

#define lua_checkwpacket(L,I)	\
	(lua_wpacket*)luaL_checkudata(L,I,WPACKET_METATABLE)

#define lua_checkrpacket(L,I)	\
	(lua_rpacket*)luaL_checkudata(L,I,RPACKET_METATABLE)

enum{
	L_TABLE = 1,
	L_STRING,
	L_BOOL,
	L_FLOAT,
	L_DOUBLE,
	L_UINT8,
	L_UINT16,
	L_UINT32,
	L_INT8,
	L_INT16,
	L_INT32,
	L_INT64,		
};

#define VAILD_KEY_TYPE(TYPE) \
	(TYPE == LUA_TSTRING || TYPE == LUA_TNUMBER)

#define VAILD_VAILD_TYPE(TYPE) \
	(TYPE == LUA_TSTRING || TYPE == LUA_TNUMBER || TYPE == LUA_TTABLE || TYPE == LUA_TBOOLEAN)	


/*
* lua_rpacket用于从一个完整的协议包中读取数据
*/


static inline int32_t lua_rpacket_read(lua_rpacket *r,char *out,uint32_t size) {
    if(size > r->data_remain) return -1;//请求数据大于剩余数据
    r->cur = chk_bytechunk_read(r->cur,out,&r->readpos,&size);
    r->data_remain -= size;
    return 0;
}

#define LUA_RPACKET_READ(R,TYPE)                                   ({\
    TYPE __result=0;                                                 \
    lua_rpacket_read(R,(char*)&__result,sizeof(TYPE));               \
    __result;                                                       })

static inline int32_t lua_rpacket_readI8(lua_State *L) {
	lua_rpacket *r = lua_checkrpacket(L,1);
    lua_pushinteger(L,LUA_RPACKET_READ(r,int8_t));
    return 1;
}

static inline int32_t lua_rpacket_readI16(lua_State *L) {
	lua_rpacket *r = lua_checkrpacket(L,1);
	int16_t v = chk_ntoh16(LUA_RPACKET_READ(r,int16_t));
    lua_pushinteger(L,v);
    return 1;
}

static inline int32_t lua_rpacket_readI32(lua_State *L) {
	lua_rpacket *r = lua_checkrpacket(L,1);
	int32_t v = chk_ntoh32(LUA_RPACKET_READ(r,int32_t));
    lua_pushinteger(L,v);
    return 1;
}

static inline int32_t lua_rpacket_readI64(lua_State *L) {
	lua_rpacket *r = lua_checkrpacket(L,1);
	int64_t v = chk_ntoh64(LUA_RPACKET_READ(r,int64_t));
    lua_pushinteger(L,v);
    return 1;
}

static inline int32_t lua_rpacket_readDub(lua_State *L) {
	lua_rpacket *r = lua_checkrpacket(L,1);
    double d = LUA_RPACKET_READ(r,double);
    //memrevifle(&d,sizeof(double));
    lua_pushnumber(L,d);
    return 1;
}

static inline int32_t lua_rpacket_readStr(lua_State *L) {
	luaL_Buffer     lb;
	char           *in;
	lua_rpacket    *r = lua_checkrpacket(L,1);
	size_t          size = (size_t)chk_ntoh32(LUA_RPACKET_READ(r,uint32_t));
	
	if(size > r->data_remain)
		return luaL_error(L,"lua_rpacket_readstr invaild packet size > r->data_remain || size == 0");

	if(size == 0) {
		//返回空串
		lua_pushstring(L,"");
		return 1;
	}

#if LUA_VERSION_NUM >= 503
	in = luaL_buffinitsize(L,&lb,size);
	if(0 != (uint32_t)lua_rpacket_read(r,in,size))
		return luaL_error(L,"lua_rpacket_readstr invaild packet");
	luaL_pushresultsize(&lb,size);
#else
	luaL_buffinit(L, &b);
	in = luaL_prepbuffsize(&b,size);
	if(0 != (uint32_t)lua_rpacket_read(r,in,size))
		return luaL_error(L,"lua_rpacket_readstr invaild packet");
	luaL_addsize(&b,size);
	luaL_pushresult(&b);
#endif
	return 1;
}

static inline int32_t lua_rpacket_readRawBytes(lua_State *L) {
	luaL_Buffer     lb;
	char           *in;
	lua_rpacket    *r = lua_checkrpacket(L,1);
	size_t          size = (size_t)r->data_remain;

	if(size == 0) {
		//返回空串
		lua_pushstring(L,"");
		return 1;
	}

#if LUA_VERSION_NUM >= 503
	in = luaL_buffinitsize(L,&lb,size);
	if(0 != (uint32_t)lua_rpacket_read(r,in,size))
		return luaL_error(L,"lua_rpacket_readRawBytes invaild packet");
	luaL_pushresultsize(&lb,size);
#else
	luaL_buffinit(L, &b);
	in = luaL_prepbuffsize(&b,size);
	if(0 != (uint32_t)lua_rpacket_read(r,in,size))
		return luaL_error(L,"lua_rpacket_readRawBytes invaild packet");
	luaL_addsize(&b,size);
	luaL_pushresult(&b);
#endif
	return 1;	
}

static inline int32_t _lua_unpack_boolean(lua_rpacket *rpk,lua_State *L) {
	lua_pushboolean(L,LUA_RPACKET_READ(rpk,uint8_t));
	return 0;
}


static inline int32_t _lua_unpack_number(lua_rpacket *rpk,lua_State *L,int type) {
	lua_Integer   n;
	switch(type){
		case L_FLOAT:
		case L_DOUBLE:
		{
			if(type == L_FLOAT) {
				float f;
				lua_rpacket_read(rpk,(char*)&f,4);
				memrevifle(&f,4);
				lua_pushnumber(L,(double)f);
			}
			else {
				double d;
				lua_rpacket_read(rpk,(char*)&d,8);
				memrevifle(&d,8);
				lua_pushnumber(L,d);				
			}
			return 0;
		}
		break;
		case L_UINT8:n  = LUA_RPACKET_READ(rpk,uint8_t);break;
		case L_UINT16:n = chk_ntoh16(LUA_RPACKET_READ(rpk,uint16_t));break;
		case L_UINT32:n = chk_ntoh32(LUA_RPACKET_READ(rpk,uint32_t));break;
		case L_INT8:n   = LUA_RPACKET_READ(rpk,int8_t);break;
		case L_INT16:n  = chk_ntoh16(LUA_RPACKET_READ(rpk,int16_t));break;
		case L_INT32:n  = chk_ntoh32(LUA_RPACKET_READ(rpk,int32_t));break;
		case L_INT64:n  = chk_ntoh64(LUA_RPACKET_READ(rpk,int64_t));break;
		default:{
			assert(0);
			return -1;						
		}
	}
	lua_pushinteger(L,n);
	return 0;
}

static inline int32_t _lua_unpack_string(lua_rpacket *rpk,lua_State *L) {
	luaL_Buffer     lb;
	char           *in;
	size_t          size = (size_t)chk_ntoh32(LUA_RPACKET_READ(rpk,uint32_t));
	
	if(size > rpk->data_remain) {
		return luaL_error(L,"size > rpk->data_remain");
	}

	if(size == 0) { 
		lua_pushstring(L,"");
		return 0;
	}

#if LUA_VERSION_NUM >= 503
	in = luaL_buffinitsize(L,&lb,size);
	if(0 != (uint32_t)lua_rpacket_read(rpk,in,size))
		return luaL_error(L,"invaild packet");
	luaL_pushresultsize(&lb,size);
#else
	luaL_buffinit(L, &b);
	in = luaL_prepbuffsize(&b,size);
	if(0 != (uint32_t)lua_rpacket_read(rpk,in,size))
		return luaL_error(L,"invaild packet");
	luaL_addsize(&b,size);
	luaL_pushresult(&b);
#endif
	return 0;
}

static int32_t _lua_unpack_table(lua_rpacket *rpk,lua_State *L) {
	int8_t key_type,value_type;
	int32_t size = chk_ntoh32(LUA_RPACKET_READ(rpk,int32_t));

	if(size > rpk->data_remain) {
		CHK_SYSLOG(LOG_ERROR,"size > rpk->data_remain");
		return -1;
	} else {
		int32_t ret,i = 0;
		lua_newtable(L);
		for(; i < size; ++i) {
			ret = -1;
			key_type = LUA_RPACKET_READ(rpk,int8_t);
			if(key_type == L_STRING)
				ret = _lua_unpack_string(rpk,L);
			else if(key_type >= L_FLOAT && key_type <= L_INT64)
				ret = _lua_unpack_number(rpk,L,key_type);
			if(ret != 0) return -1;
			ret = -1;
			value_type = LUA_RPACKET_READ(rpk,int8_t);
			if(value_type == L_STRING)
				ret = _lua_unpack_string(rpk,L);
			else if(value_type >= L_FLOAT && value_type <= L_INT64)
				ret = _lua_unpack_number(rpk,L,value_type);
			else if(value_type == L_BOOL)
				ret = _lua_unpack_boolean(rpk,L);
			else if(value_type == L_TABLE)
				ret = _lua_unpack_table(rpk,L);
			if(ret != 0) return -1;
			lua_rawset(L,-3);			
		}
		return 0;
	}
}


static inline int32_t lua_rpacket_readTable(lua_State *L) {
	lua_rpacket    *r = lua_checkrpacket(L,1);
	if(L_TABLE != LUA_RPACKET_READ(r,int8_t))
		return luaL_error(L,"invaild packet");
	if(0 != _lua_unpack_table(r,L))	
		return luaL_error(L,"invaild packet");
	return 1;
}

/*
* lua_wpacket用于向buffer中写入符合协议的数据
*/

static inline int32_t lua_wpacket_write(lua_wpacket *w,char *in,uint32_t size) {
    if(w->buff->datasize + size < w->buff->datasize) {
    	//log
    	return -1;
    }
    chk_bytebuffer_append(w->buff,(uint8_t*)in,size);
    *((uint32_t*)(w->buff->head->data)) = chk_hton32(w->buff->datasize - sizeof(uint32_t));
    return 0;
}

static inline int32_t lua_wpacket_writeI8(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);
	int8_t value = (int8_t)luaL_checkinteger(L,2);  
    if(0 != lua_wpacket_write(w,(char*)&value,sizeof(value)))
    	return luaL_error(L,"write beyond limited");
    return 0;
}

static inline int32_t lua_wpacket_writeI16(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1); 
    int16_t value = chk_hton16((int16_t)luaL_checkinteger(L,2));
    if(0 != lua_wpacket_write(w,(char*)&value,sizeof(value)))
    	return luaL_error(L,"write beyond limited");
    return 0;       
}

static inline int32_t lua_wpacket_writeI32(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);    
    int32_t value = chk_hton32((int32_t)luaL_checkinteger(L,2));
    if(0 != lua_wpacket_write(w,(char*)&value,sizeof(value)))
    	return luaL_error(L,"write beyond limited");
    return 0;
}

static inline int32_t lua_wpacket_writeI64(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);    
    int64_t value = chk_hton64((int64_t)luaL_checkinteger(L,2));
    if(0 != lua_wpacket_write(w,(char*)&value,sizeof(value)))
    	return luaL_error(L,"write beyond limited");
    return 0;
}

static inline int32_t lua_wpacket_writeDub(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);
	double value = luaL_checknumber(L,2);  
	//memrevifle(&value,sizeof(double));  
    if(0 != lua_wpacket_write(w,(char*)&value,sizeof(value)))
    	return luaL_error(L,"write beyond limited");
    return 0;
}

static inline int32_t lua_wpacket_get_write_pos(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);
	lua_pushinteger(L,w->buff->datasize);
	return 1;
} 

static inline int32_t lua_wpacket_rewriteI8(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);	
	uint32_t pos   = luaL_checkinteger(L,2);
	int8_t v = (int8_t)luaL_checkinteger(L,3);	
	if(pos + sizeof(int8_t) > w->buff->datasize) {
    	return luaL_error(L,"invaild write pos"); 
	}
	if(chk_error_ok != chk_bytebuffer_rewrite(w->buff,pos,(uint8_t*)&v,(uint32_t)sizeof(v))) {
		return luaL_error(L,"invaild write pos");
	}
	return 0;
}

static inline int32_t lua_wpacket_rewriteI16(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);	
	uint32_t pos   = luaL_checkinteger(L,2);
	int16_t v = chk_hton16((int16_t)luaL_checkinteger(L,3));	
	if(pos + sizeof(int16_t) > w->buff->datasize) {
    	return luaL_error(L,"invaild write pos");
	}
	if(chk_error_ok != chk_bytebuffer_rewrite(w->buff,pos,(uint8_t*)&v,(uint32_t)sizeof(v))) {
		return luaL_error(L,"invaild write pos");
	}
	return 0;	
}

static inline int32_t lua_wpacket_rewriteI32(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);	
	uint32_t pos   = luaL_checkinteger(L,2);
	int32_t v = chk_hton32((int32_t)luaL_checkinteger(L,3));
	if(pos + sizeof(int32_t) > w->buff->datasize) {
    	return luaL_error(L,"invaild write pos"); 
	}
	if(chk_error_ok != chk_bytebuffer_rewrite(w->buff,pos,(uint8_t*)&v,(uint32_t)sizeof(v))) {
		return luaL_error(L,"invaild write pos");
	}
	return 0;	
}

static inline int32_t lua_wpacket_rewriteI64(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);	
	uint32_t pos   = luaL_checkinteger(L,2);
	int64_t v = chk_hton64((int64_t)luaL_checkinteger(L,3));
	if(pos + sizeof(int64_t) > w->buff->datasize) {
    	return luaL_error(L,"invaild write pos");
	}
	if(chk_error_ok != chk_bytebuffer_rewrite(w->buff,pos,(uint8_t*)&v,(uint32_t)sizeof(v))) {
		return luaL_error(L,"invaild write pos");
	}
	return 0;	
}

static inline int32_t lua_wpacket_writeStr(lua_State *L) {
	const char  *str;
	size_t       len;
	uint32_t     size;
	int32_t      ret;	
	lua_wpacket *w = lua_checkwpacket(L,1);
	if(!lua_isstring(L,2)) luaL_error(L,"argument 2 or lua_rpacket_readstr must be string");
	str = lua_tolstring(L,2,&len);
	size = chk_hton32((uint32_t)len);
	do{
		if(0 != (ret = lua_wpacket_write(w,(char*)&size,sizeof(size))))
			break;
		if(len > 0){
			if(0 != (ret = lua_wpacket_write(w,(char*)str,len)))
				break;
		}		
	}while(0);

	if(0 != ret)
		return luaL_error(L,"write beyond limited");

	return 0;
}

static inline int32_t lua_wpacket_writeRawBytes(lua_State *L) {
	const char  *str;
	size_t       len;
	int32_t      ret = -1;	
	lua_wpacket *w = lua_checkwpacket(L,1);
	if(!lua_isstring(L,2)) luaL_error(L,"argument 2 or lua_rpacket_readstr must be string");
	str = lua_tolstring(L,2,&len);
	do{
		if(len > 0){
			if(0 != (ret = lua_wpacket_write(w,(char*)str,len)))
				break;
		}		
	}while(0);

	if(0 != ret)
		return luaL_error(L,"write beyond limited");

	return 0;	
}


static int32_t encode_double(lua_wpacket *wpk, double d) {
    unsigned char b[9];
    float f = d;
    if (d == (double)f) {
        b[0] = L_FLOAT;     /* float IEEE 754 */
        memcpy(b+1,&f,4);
        memrevifle(b+1,4);
        return lua_wpacket_write(wpk,(char*)b,5);
    } else if (sizeof(d) == 8) {
        b[0] = L_DOUBLE;    /* double IEEE 754 */
        memcpy(b+1,&d,8);
        memrevifle(b+1,8);
        return lua_wpacket_write(wpk,(char*)b,9); 
    }
}

static inline int32_t _lua_pack_string(lua_wpacket *wpk,lua_State *L,int index) {
	size_t      len;
	uint32_t    size;
	const char *data;
	int8_t      type = L_STRING;

	if(0 != lua_wpacket_write(wpk,(char*)&type,sizeof(type)))
		return -1;
	data = lua_tolstring(L,index,&len);
	size = chk_hton32((uint32_t)len);
	
	if(0 != lua_wpacket_write(wpk,(char*)&size,sizeof(size)))
		return -1;
	if(len > 0){
		if(0 != lua_wpacket_write(wpk,(char*)data,len))
			return -1;
	}
	return 0;
}

static inline int32_t _lua_pack_boolean(lua_wpacket *wpk,lua_State *L,int index) {
	
	int8_t type = L_BOOL;
	int8_t value = (int8_t)lua_toboolean(L,index);

	if(0 != lua_wpacket_write(wpk,(char*)&type,sizeof(type)))
		return -1;
	if(0 != lua_wpacket_write(wpk,(char*)&value,value))
		return -1;
	return 0;	
}

static  int32_t _lua_pack_number(lua_wpacket *wpk,lua_State *L,int index) {
	int32_t    ret;
	int8_t     type;
	int64_t    vi64;
	uint64_t   vu64;
	int32_t    vi32;
	uint32_t   vu32;
	int16_t    vi16;
	uint16_t   vu16;
	int8_t     vi8;
	uint8_t    vu8;

	lua_Number v = lua_tonumber(L,index);
	do{
		if(v != cast(lua_Integer,v)) {
			if(0 != (ret = encode_double(wpk,v))) {
				break;
			}
		}else {
			if(cast(int64_t,v) > 0){
				vu64 = cast(uint64_t,v);
				if(vu64 <= 0xFF){
					type = L_UINT8;
					vu8 = (uint8_t)vu64;					
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&type,sizeof(type))))
						break;	
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&vu8,sizeof(vu8))))
						break;				
				}else if(vu64 <= 0xFFFF){
					type = L_UINT16;
					vu16 = chk_hton16((uint16_t)vu64);
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&type,sizeof(type))))
						break;	
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&vu16,sizeof(vu16))))
						break;						
				}else if(vu64 <= 0xFFFFFFFF){
					type = L_UINT32;
					vu32 = chk_hton32((uint32_t)vu64);
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&type,sizeof(type))))
						break;	
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&vu32,sizeof(vu32))))
						break;										
				}else{
					type = L_INT64;
					vi64 = chk_hton64((int64_t)vu64);
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&type,sizeof(type))))
						break;	
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&vi64,sizeof(vi64))))
						break;				
				}
			}else{
				int64_t vi64 = (int64_t)v;
				if(vi64 <= 0x80){
					type = L_INT8;
					vi8 = (int8_t)vi64;
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&type,sizeof(type))))
						break;	
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&vi8,sizeof(vi8))))
						break;						
				}else if(vi64 <= 0x8000){
					type = L_INT16;
					vi16 = chk_hton16((int16_t)vi64);
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&type,sizeof(type))))
						break;	
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&vi16,sizeof(vi16))))
						break;						
				}else if(vi64 < 0x80000000){
					type = L_INT32;
					vi32 = chk_hton32((int32_t)vi64);
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&type,sizeof(type))))
						break;	
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&vi32,sizeof(vi32))))
						break;					
				}else{
					type = L_INT64;
					vi64 = chk_hton64((int64_t)vi64);
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&type,sizeof(type))))
						break;	
					if(0 != (ret = lua_wpacket_write(wpk,(char*)&vi64,sizeof(vi64))))
						break;				
				}
			}
		}
	}while(0);

	if(0 != ret)
		return luaL_error(L,"write beyond limited");
	return 0;
}

static int32_t _lua_pack_table(lua_wpacket *wpk,lua_State *L,int index) {
	int32_t c,top,ret;
	chk_bytechunk *chunk;
	int8_t  type;
	uint32_t pos,size = sizeof(c);
	check_cycle(L,index);
	top = lua_gettop(L);
	type = L_TABLE;
	
	if(0 != lua_wpacket_write(wpk,(char*)&type,sizeof(type)))
		return -1;		
	
	chunk = wpk->buff->tail;
	pos   = wpk->buff->append_pos;
	c 	  = 0;
	
	if(0 != lua_wpacket_write(wpk,(char*)&c,sizeof(c)))
		return -1;	
	
	lua_pushnil(L);

	ret = 0;

	for(;;){		
		if(!lua_next(L,index - 1)){
			break;
		}
		int32_t key_type = lua_type(L, -2);
		int32_t val_type = lua_type(L, -1);
		if(VAILD_KEY_TYPE(key_type) && VAILD_VAILD_TYPE(val_type)){
			if(key_type == LUA_TSTRING) {
				if(0 != (ret = _lua_pack_string(wpk,L,-2)))
					break;
			}
			else {
				if(0 != (ret = _lua_pack_number(wpk,L,-2)))
					break;
			}

			if(val_type == LUA_TSTRING) {
				if(0 != (ret = _lua_pack_string(wpk,L,-1)))
					break;
			}
			else if(val_type == LUA_TNUMBER) {
				if(0 != (ret = _lua_pack_number(wpk,L,-1)))
					break;
			}
			else if(val_type == LUA_TBOOLEAN) {
				if(0 != (ret = _lua_pack_boolean(wpk,L,-1)))
					break;
			}
			else if(val_type == LUA_TTABLE) {
				if(0 != lua_getmetatable(L,index)){ 
					ret = -1;
					break;
				}
				if(0 != (ret = _lua_pack_table(wpk,L,-1)))
					break;
			}else {
				ret = -1;
			}
			++c;
		}
		lua_pop(L,1);
	}

	if(0 == ret) {
		lua_settop(L,top);
		c = chk_hton32(c);
		chk_bytechunk_write(chunk,cast(char*,&c),&pos,&size);
		return 0;
	}

	return -1;
}

static inline int32_t lua_wpacket_writeTable(lua_State *L) {
	int32_t ret;
	lua_wpacket *w = lua_checkwpacket(L,1);
	if(lua_type(L, 2) != LUA_TTABLE)
		return luaL_error(L,"argumen 2 of wpacket_writeTable must be table");
	ret = _lua_pack_table(w,L,-1);
	clean_cycle_check_list();
	if(0 != ret) {
		return luaL_error(L,"write table error");
	}
	return 0;
}

/////

static inline int32_t lua_new_wpacket(lua_State *L) {
	lua_wpacket    *w = NULL;
	chk_bytebuffer *buff = lua_checkbytebuffer(L,1);
	if(buff->flags & READ_ONLY){
		luaL_error(L,"buffer is readonly");
	}	
	w = LUA_NEWUSERDATA(L,lua_wpacket);
	if(!w) return 0;
	w->buff = buff;
	chk_bytebuffer_append_dword(buff,0);
	luaL_getmetatable(L, WPACKET_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static inline int32_t lua_new_rpacket(lua_State *L) {
	chk_bytebuffer *buff = lua_checkbytebuffer(L,1);
	lua_rpacket    *r = LUA_NEWUSERDATA(L,lua_rpacket);
	if(!r) return 0;
	r->cur = buff->head;
	r->buff = buff;
	r->data_remain = buff->datasize - sizeof(uint32_t);
	r->total_packet_size = buff->datasize;
	switch(r->cur->cap - buff->spos) {
		case 1:{r->cur = r->cur->next;r->readpos = 3;}break;
		case 2:{r->cur = r->cur->next;r->readpos = 2;}break;
		case 3:{r->cur = r->cur->next;r->readpos = 1;}break;
		default:r->readpos = buff->spos + 4;
	}
	luaL_getmetatable(L, RPACKET_METATABLE);
	lua_setmetatable(L, -2);	
	return 1;
}

static inline int32_t lua_new_decoder(lua_State *L) {
	uint32_t max = (uint32_t)luaL_optinteger(L,1,1024);
	packet_decoder *d = packet_decoder_new(max);
	if(d){
		lua_pushlightuserdata(L,d);
		return 1;
	}
	else
		return 0;
}

static void register_packet(lua_State *L) {

	luaL_Reg wpacket_methods[] = {
		{"WriteI8",   lua_wpacket_writeI8},
		{"WriteI16",  lua_wpacket_writeI16},
		{"WriteI32",  lua_wpacket_writeI32},
		{"WriteI64",  lua_wpacket_writeI64},
		{"WriteNum",  lua_wpacket_writeDub},
		{"WriteStr",  lua_wpacket_writeStr},
		{"WriteRawBytes",lua_wpacket_writeRawBytes},
		{"WriteTable",lua_wpacket_writeTable},
		{"GetWritePos",lua_wpacket_get_write_pos},
		{"ReWriteI8",lua_wpacket_rewriteI8},
		{"ReWriteI16",lua_wpacket_rewriteI16},
		{"ReWriteI32",lua_wpacket_rewriteI32},
		{"ReWriteI64",lua_wpacket_rewriteI64},								
		{NULL,     NULL}
	};

	luaL_Reg rpacket_methods[] = {
		{"ReadI8",   lua_rpacket_readI8},
		{"ReadI16",  lua_rpacket_readI16},
		{"ReadI32",  lua_rpacket_readI32},
		{"ReadI64",  lua_rpacket_readI64},
		{"ReadNum",  lua_rpacket_readDub},
		{"ReadStr",  lua_rpacket_readStr},
		{"ReadRawBytes",  lua_rpacket_readRawBytes},
		{"ReadTable",  lua_rpacket_readTable},
		{NULL,     NULL}
	};	

	luaL_newmetatable(L, WPACKET_METATABLE);
	luaL_newlib(L, wpacket_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);


	luaL_newmetatable(L, RPACKET_METATABLE);
	luaL_newlib(L, rpacket_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_newtable(L);
	SET_FUNCTION(L,"Writer",lua_new_wpacket);
	SET_FUNCTION(L,"Reader",lua_new_rpacket);
	SET_FUNCTION(L,"Decoder",lua_new_decoder);

}