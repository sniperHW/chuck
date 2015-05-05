#include "socket/wrap/decoder.h"
#include "packet/rpacket.h"

static packet *rpk_unpack(decoder *d,int32_t *err){
	TYPE_HEAD     pk_len;
	uint32_t      pk_total,size;
	buffer_reader reader;
	rpacket      *rpk;
	if(err) *err = 0;

	if(d->size <= SIZE_HEAD)
		return NULL;

	buffer_reader_init(&reader,d->buff,d->pos);
	if(SIZE_HEAD != buffer_read(&reader,(char*)&pk_len,SIZE_HEAD))
		return NULL;
	pk_len = _ntoh16(pk_len);
	pk_total = pk_len + SIZE_HEAD;
	if(pk_total > d->max_packet_size){
		if(err) *err = DERR_TOOLARGE;
		return NULL;
	}
	if(pk_total > d->size)
		return NULL;

	rpk = rpacket_new(d->buff,d->pos);

	do{
		size = d->buff->size - d->pos;
		size = pk_total > size ? size:pk_total;
		d->pos    += size;
		pk_total  -= size;
		d->size -= size;
		if(d->pos >= d->buff->cap)
		{
			bytebuffer_set(&d->buff,d->buff->next);
			d->pos = 0;
			if(!d->buff){
				assert(pk_total == 0);
				break;
			}
		}
	}while(pk_total);
	return (packet*)rpk;
}


decoder *rpacket_decoder_new(uint32_t max_packet_size){
	decoder *d = calloc(1,sizeof(*d));
	d->unpack = rpk_unpack;
	d->max_packet_size = max_packet_size;
	return d;
}

void decoder_del(decoder *d){
	if(d->dctor) d->dctor(d);
	if(d->buff) bytebuffer_set(&d->buff,NULL);
	free(d);
}


int32_t lua_rpacket_decoder_new(lua_State *L){
	if(LUA_TNUMBER != lua_type(L,1))
		return luaL_error(L,"arg1 should be number");
	lua_pushlightuserdata(L,rpacket_decoder_new(lua_tonumber(L,1)));
	return 1;
}

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)

void reg_luadecoder(lua_State *L){
	lua_newtable(L);
	SET_FUNCTION(L,"rpacket",lua_rpacket_decoder_new);
}