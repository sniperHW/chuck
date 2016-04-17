/*
*  数据包表示       |4字节数据长度|数据...|
*  二进制数据表示法	|4字节数据长度|数据...|
*/


//#include "lua/chk_lua.h"
//#include "util/chk_error.h"
//#include "util/chk_bytechunk.h"
//#include "util/chk_order.h"
//#include "socket/chk_decoder.h"

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
}lua_rpacket;


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


//一个解包器,包头4字节,表示后面数据大小.
typedef struct _decoder {
	void (*update)(chk_decoder*,chk_bytechunk *b,uint32_t spos,uint32_t size);
	chk_bytebuffer *(*unpack)(chk_decoder*,int32_t *err);
	void (*dctor)(chk_decoder*);
	uint32_t       spos;
	uint32_t       size;
	uint32_t       max;
	chk_bytechunk *b;
}_decoder;

static inline void _update(chk_decoder *_,chk_bytechunk *b,uint32_t spos,uint32_t size) {
	_decoder *d = ((_decoder*)_);
    if(!d->b) {
	    d->b = chk_bytechunk_retain(b);
	    d->spos  = spos;
	    d->size = 0;
    }
    d->size += size;
}

static inline chk_bytebuffer *_unpack(chk_decoder *_,int32_t *err) {
	_decoder *d = ((_decoder*)_);
	chk_bytebuffer *ret  = NULL;
	chk_bytechunk  *head = d->b;
	uint32_t        pk_len;
	uint32_t        pk_total,size,pos;
	do {
		if(d->size <= sizeof(pk_len)) break;
		size = sizeof(pk_len);
		pos  = d->spos;
		chk_bytechunk_read(head,(char*)&pk_len,&pos,&size);//读取payload大小
		pk_len = chk_ntoh32(pk_len);
		if(pk_len == 0) {
			if(err) *err = -1;
			break;
		}
		pk_total = size + pk_len;
		if(pk_total > d->max) {
			if(err) *err = CHK_EPKTOOLARGE;//数据包操作限制大小
			break;
		}
		if(pk_total > d->size) break;//没有足够的数据
		ret = chk_bytebuffer_new_bychunk(head,d->spos,pk_total);
		//调整pos及其b
		do {
			head = d->b;
			size = head->cap - d->spos;
			size = pk_total > size ? size:pk_total;
			d->spos  += size;
			pk_total-= size;
			d->size -= size;
			if(d->spos >= head->cap) { //当前b数据已经用完
				d->b = chk_bytechunk_retain(head->next);
				chk_bytechunk_release(head);
				d->spos = 0;
				if(!d->b) break;
			}
		}while(pk_total);			
	}while(0);
	return ret;
}

static inline void _dctor(chk_decoder *_) {
	_decoder *d = ((_decoder*)_);
	if(d->b) chk_bytechunk_release(d->b);
}

static inline _decoder *_decoder_new(uint32_t max) {
	_decoder *d = calloc(1,sizeof(*d));
	d->update = _update;
	d->unpack = _unpack;
	d->max    = max;
	d->dctor  = _dctor;
	return d;
}


/*
* lua_rpacket用于从一个完整的协议包中读取数据
*/


static inline int32_t lua_rpacket_read(lua_rpacket *r,char *out,uint32_t size) {
    if(size > r->data_remain) return -1;//请求数据大于剩余数据
    r->cur      = chk_bytechunk_read(r->cur,out,&r->readpos,&size);
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
	int64_t v = chk_ntoh32(LUA_RPACKET_READ(r,int64_t));
    lua_pushinteger(L,v);
    return 1;
}

static inline int32_t lua_rpacket_readDub(lua_State *L) {
	lua_rpacket *r = lua_checkrpacket(L,1);
    lua_pushnumber(L,LUA_RPACKET_READ(r,double));
    return 1;
}

static inline int32_t lua_rpacket_readStr(lua_State *L) {
	luaL_Buffer     lb;
	char           *in;
	lua_rpacket    *r = lua_checkrpacket(L,1);
	size_t          size = (size_t)chk_ntoh32(LUA_RPACKET_READ(r,uint32_t));
	if(size == 0) return 0;

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

static inline int32_t _lua_unpack_boolean(lua_rpacket *rpk,lua_State *L) {
	lua_pushboolean(L,LUA_RPACKET_READ(rpk,uint8_t));
	return 0;
}

static inline int32_t _lua_unpack_number(lua_rpacket *rpk,lua_State *L,int type) {
	lua_Integer   n;
	switch(type){
		case L_FLOAT:lua_pushnumber(L, LUA_RPACKET_READ(rpk,float));return 0;
		case L_DOUBLE:lua_pushnumber(L, LUA_RPACKET_READ(rpk,double));return 0;
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
	if(size <= 0) return -1;

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

#define CHECK_WRITE_NUM(W,V,T)										\
	do{																\
		T _V = (V);													\
		if(0!= lua_wpacket_write((W),(char*)&(_V),sizeof(T))) 		\
			return luaL_error(L,"write beyond limited");			\
	}while(0)

#define CHECK_WRITE_STR(W,V,S)										\
	do{																\
		if(0!= lua_wpacket_write((W),(char*)(V),(uint32_t)(S)))		\
			return luaL_error(L,"write beyond limited");			\
	}while(0)

#define CHECK_WRITE_PACK(FUNC,W,L,I)								\
	do{																\
		if(0!= FUNC(W,L,I))											\
			return luaL_error(L,"write beyond limited");			\
	}while(0)


static inline int32_t lua_wpacket_writeI8(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);
	int8_t value = (int8_t)luaL_checkinteger(L,2);  
    CHECK_WRITE_NUM(w,value,int8_t);
    return 0;
}

static inline int32_t lua_wpacket_writeI16(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1); 
    int16_t value = chk_hton16((int16_t)luaL_checkinteger(L,2));
    CHECK_WRITE_NUM(w,value,int16_t);
    return 0;       
}

static inline int32_t lua_wpacket_writeI32(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);    
    int32_t value = chk_hton32((int32_t)luaL_checkinteger(L,2));
    CHECK_WRITE_NUM(w,value,int32_t);
    return 0;
}

static inline int32_t lua_wpacket_writeI64(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);    
    int64_t value = chk_hton64((int64_t)luaL_checkinteger(L,2));
    CHECK_WRITE_NUM(w,value,int64_t);
    return 0;
}

static inline int32_t lua_wpacket_writeDub(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);
	double value = luaL_checknumber(L,2);    
    CHECK_WRITE_NUM(w,value,double);
    return 0;
}

static inline int32_t lua_wpacket_writeStr(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);
	const char  *str;
	size_t       len;
	uint32_t     size;
	if(!lua_isstring(L,2)) luaL_error(L,"argument 2 or lua_rpacket_readstr must be string");
	str = lua_tolstring(L,2,&len);
	size = chk_hton32((uint32_t)len);
	CHECK_WRITE_NUM(w,size,uint32_t);
	CHECK_WRITE_STR(w,str,len);
	return 0;
}


static inline int32_t _lua_pack_string(lua_wpacket *wpk,lua_State *L,int index) {
	size_t      len;
	uint32_t    size;
	const char *data;
	CHECK_WRITE_NUM(wpk,L_STRING,int8_t);
	data = lua_tolstring(L,index,&len);
	size = chk_hton32((uint32_t)len);
	CHECK_WRITE_NUM(wpk,size,uint32_t);
	CHECK_WRITE_STR(wpk,data,len);
	return 0;	
}

static inline int32_t _lua_pack_boolean(lua_wpacket *wpk,lua_State *L,int index) {
	CHECK_WRITE_NUM(wpk,L_BOOL,int8_t);
	CHECK_WRITE_NUM(wpk,lua_toboolean(L,index),int8_t);
	return 0;
}


static  int32_t _lua_pack_number(lua_wpacket *wpk,lua_State *L,int index) {
	lua_Number v = lua_tonumber(L,index);
	if(v != cast(lua_Integer,v)){
		if(v != cast(float,v)) {
			CHECK_WRITE_NUM(wpk,L_FLOAT,int8_t);
			CHECK_WRITE_NUM(wpk,v,float);
		}else {
			CHECK_WRITE_NUM(wpk,L_DOUBLE,int8_t);
			CHECK_WRITE_NUM(wpk,v,double);
		}
	}else{
		if(cast(int64_t,v) > 0){
			uint64_t _v = cast(uint64_t,v);
			if(_v <= 0xFF){
				CHECK_WRITE_NUM(wpk,L_UINT8,int8_t);
				CHECK_WRITE_NUM(wpk,cast(uint8_t,_v),uint8_t);				
			}else if(_v <= 0xFFFF){
				CHECK_WRITE_NUM(wpk,L_UINT16,int8_t);
				CHECK_WRITE_NUM(wpk,cast(uint16_t,chk_hton16(_v)),uint16_t);					
			}else if(_v <= 0xFFFFFFFF){
				CHECK_WRITE_NUM(wpk,L_UINT32,int8_t);
				CHECK_WRITE_NUM(wpk,cast(uint32_t,chk_hton32(_v)),uint32_t);					
			}else{
				CHECK_WRITE_NUM(wpk,L_INT64,int8_t);
				CHECK_WRITE_NUM(wpk,cast(int64_t,chk_hton64(_v)),int64_t);				
			}
		}else{
			int64_t _v = cast(int64_t,v);
			if(_v <= 0x80){
				CHECK_WRITE_NUM(wpk,L_INT8,int8_t);
				CHECK_WRITE_NUM(wpk,cast(int8_t,_v),int8_t);				
			}else if(_v <= 0x8000){
				CHECK_WRITE_NUM(wpk,L_INT16,int8_t);
				CHECK_WRITE_NUM(wpk,cast(int16_t,chk_hton16(_v)),int16_t);					
			}else if(_v < 0x80000000){
				CHECK_WRITE_NUM(wpk,L_INT32,int8_t);
				CHECK_WRITE_NUM(wpk,cast(int32_t,chk_hton32(_v)),int32_t);					
			}else{
				CHECK_WRITE_NUM(wpk,L_INT64,int8_t);
				CHECK_WRITE_NUM(wpk,cast(int64_t,chk_hton64(_v)),int64_t);				
			}
		}
	}
	return 0;
}

static const int32_t _lua_pack_table(lua_wpacket *wpk,lua_State *L,int index) {
	int32_t c   = 0;
	int32_t top = lua_gettop(L);
	chk_bytechunk *chunk;
	uint32_t pos,size = sizeof(c);
	CHECK_WRITE_NUM(wpk,L_TABLE,int8_t);
	chunk = wpk->buff->tail;
	pos   = wpk->buff->append_pos;
	CHECK_WRITE_NUM(wpk,0,int32_t);
	lua_pushnil(L);
	do{		
		if(!lua_next(L,index - 1)){
			break;
		}
		int32_t key_type = lua_type(L, -2);
		int32_t val_type = lua_type(L, -1);
		if(VAILD_KEY_TYPE(key_type) && VAILD_VAILD_TYPE(val_type)){
			if(key_type == LUA_TSTRING)
				CHECK_WRITE_PACK(_lua_pack_string,wpk,L,-2);
			else
				CHECK_WRITE_PACK(_lua_pack_number,wpk,L,-2);

			if(val_type == LUA_TSTRING)
				CHECK_WRITE_PACK(_lua_pack_string,wpk,L,-1);
			else if(val_type == LUA_TNUMBER)
				CHECK_WRITE_PACK(_lua_pack_number,wpk,L,-1);
			else if(val_type == LUA_TBOOLEAN)
				CHECK_WRITE_PACK(_lua_pack_boolean,wpk,L,-1);
			else if(val_type == LUA_TTABLE){
				if(0 != lua_getmetatable(L,index)){ 
					return luaL_error(L,"contain metatable");
				}
				CHECK_WRITE_PACK(_lua_pack_table,wpk,L,-1);
			}else{
				return luaL_error(L,"unsupport type");
			}
			++c;
		}
		lua_pop(L,1);
	}while(1);
	lua_settop(L,top);
	c = chk_hton32(c);
	chk_bytechunk_write(chunk,cast(char*,&c),&pos,&size);
	return 0;
}

static inline int32_t lua_wpacket_writeTable(lua_State *L) {
	lua_wpacket *w = lua_checkwpacket(L,1);
	if(lua_type(L, 2) != LUA_TTABLE)
		luaL_error(L,"argumen 2 of wpacket_writeTable must be table");
	return _lua_pack_table(w,L,-1);
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
	switch(r->cur->cap - buff->spos) {
		case 1:{r->cur = r->cur->next;r->readpos = 3;}break;
		case 2:{r->cur = r->cur->next;r->readpos = 2;}break;
		case 3:{r->cur = r->cur->next;r->readpos = 2;}break;
		default:r->readpos = buff->spos + 4;
	}
	luaL_getmetatable(L, RPACKET_METATABLE);
	lua_setmetatable(L, -2);	
	return 1;
}

static inline int32_t lua_new_decoder(lua_State *L) {
	uint32_t max = (uint32_t)luaL_optinteger(L,1,1024);
	lua_pushlightuserdata(L,_decoder_new(max));
	return 1;
}

static void register_packet(lua_State *L) {

	luaL_Reg wpacket_methods[] = {
		{"WriteI8",   lua_wpacket_writeI8},
		{"WriteI16",  lua_wpacket_writeI16},
		{"WriteI32",  lua_wpacket_writeI32},
		{"WriteI64",  lua_wpacket_writeI64},
		{"WriteNum",  lua_wpacket_writeDub},
		{"WriteStr",  lua_wpacket_writeStr},
		{"WriteTable",lua_wpacket_writeTable},
		{NULL,     NULL}
	};

	luaL_Reg rpacket_methods[] = {
		{"ReadI8",   lua_rpacket_readI8},
		{"ReadI16",  lua_rpacket_readI16},
		{"ReadI32",  lua_rpacket_readI32},
		{"ReadI64",  lua_rpacket_readI64},
		{"ReadNum",  lua_rpacket_readDub},
		{"ReadStr",  lua_rpacket_readStr},
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