#include "socket/wrap/decoder.h"
#include "packet/rpacket.h"
#include "packet/rawpacket.h"

static inline void 
_decoder_update(decoder *d,bytebuffer *buff,
               uint32_t pos,uint32_t size)
{
    if(!d->buff){
	    d->buff = buff;
	    refobj_inc((refobj*)buff);
	    d->pos  = pos;
	    d->size = 0;
    }
    d->size += size;
}

static packet*
rpk_unpack(decoder *d,int32_t *err)
{
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
		if(err) *err = EPKTOOLARGE;
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


decoder*
rpacket_decoder_new(uint32_t max_packet_size)
{
	decoder *d = calloc(1,sizeof(*d));
	d->decoder_update = _decoder_update;
	d->unpack = rpk_unpack;
	d->max_packet_size = max_packet_size;
	return d;
}

static packet*
conn_rawpk_unpack(decoder *d,int32_t *err)
{
    rawpacket  *raw;
    uint32_t    size;
    if(err) *err = 0;
    if(!d->size) return NULL;

    assert(d->buff->size > d->pos);
    raw = rawpacket_new_by_buffer(d->buff,d->pos);
    size = d->buff->size - d->pos;
    assert(d->size >= size);
    d->pos  += size;
    d->size -= size;
    ((packet*)raw)->len_packet = size;
    if(d->pos >= d->buff->cap){
        d->pos = 0;
        bytebuffer_set(&d->buff,d->buff->next);
    }
    return (packet*)raw;
}

decoder*
conn_raw_decoder_new()
{
    decoder *d = calloc(1,sizeof(*d));
	d->decoder_update = _decoder_update;    
    d->unpack = conn_rawpk_unpack;
    return d;   
}

static packet*
dgram_rawpk_unpack(decoder *d,int32_t *err)
{
	rawpacket    *raw;
	uint32_t      size;
	buffer_writer writer;
	bytebuffer   *buff;
	if(err) *err = 0;
	if(!d->size) return NULL;
	raw = rawpacket_new(d->size);
	buffer_writer_init(&writer,((packet*)raw)->head,0);
	((packet*)raw)->len_packet = d->size;
	while(d->size){
		buff = d->buff;
		size = buff->size - d->pos;
		buffer_write(&writer,buff->data + d->pos,size);
		d->size -= size;
		d->pos += size;
		if(d->pos >= buff->cap){
			d->pos = 0;
			bytebuffer_set(&d->buff,buff->next);
		}
	}
	return (packet*)raw;
}

decoder*
dgram_raw_decoder_new()
{
	decoder *d = calloc(1,sizeof(*d));
	d->unpack = dgram_rawpk_unpack;
	d->decoder_update = _decoder_update; 	
	return d;	
}

static inline httpdecoder*
cast2httpdecoder(http_parser *parser)
{
	return cast(httpdecoder*,((char*)parser - sizeof(decoder)));
}

static int 
on_message_begin (http_parser *parser)
{
	httpdecoder *decoder = cast2httpdecoder(parser);
	if(decoder->packet){  return -1; printf("error here\n");}
	decoder->packet      = httppacket_new(decoder->buff);
	return 0;
}

static int 
on_url(http_parser *parser, const char *at, size_t length)
{	
	httpdecoder *decoder   = cast2httpdecoder(parser);
	cast(char*,at)[length] = 0;
	decoder->packet->url   = at - &decoder->buff->data[0];
	return 0;
}

static int 
on_status(http_parser *parser, const char *at, size_t length)
{
	httpdecoder *decoder    = cast2httpdecoder(parser);
	cast(char*,at)[length]  = 0;
	decoder->packet->status = at - &decoder->buff->data[0];
	return 0;			
}

static int 
on_header_field(http_parser *parser, const char *at, size_t length)
{
	httpdecoder *decoder   = cast2httpdecoder(parser);
	cast(char*,at)[length] = 0;
	return httppacket_on_header_field(decoder->packet,cast(char*,at),length);				
}

static int 
on_header_value(http_parser *parser, const char *at, size_t length)
{
	httpdecoder *decoder   = cast2httpdecoder(parser);
	cast(char*,at)[length] = 0;
	return httppacket_on_header_value(decoder->packet,cast(char*,at),length);			
}

static int 
on_headers_complete(http_parser *parser)
{	
	httpdecoder *decoder    = cast2httpdecoder(parser);
	decoder->packet->method = parser->method;
	return 0;		
}

static int 
on_body(http_parser *parser, const char *at, size_t length)
{
	httpdecoder *decoder      = cast2httpdecoder(parser);
	cast(char*,at)[length]    = 0;
	decoder->packet->body     = at - &decoder->buff->data[0];
	decoder->packet->bodysize = length;
	return 0;		
}


enum{
	HTTP_COMPLETE = 1,
	HTTP_TOOLARGE = 2,
};

static int 
on_message_complete(http_parser *parser)
{	
	httpdecoder *decoder = cast2httpdecoder(parser);
	decoder->status      = HTTP_COMPLETE;
	return 0;							
}


static inline int32_t
http_decoder_expand(httpdecoder *d,uint32_t size)
{
	bytebuffer *newbuff;
	uint32_t newsize = size_of_pow2(size);
	if(newsize < d->buff->cap || newsize > d->max_packet_size)
		return -1;	
    newbuff = bytebuffer_new(newsize);
   	memcpy(newbuff->data,d->buff->data,d->buff->size);
   	newbuff->size = d->buff->size;
   	bytebuffer_set(&d->buff,newbuff);
   	if(d->packet) httppacket_on_buffer_expand(d->packet,d->buff);
   	return 0;
}

static inline void 
http_decoder_update(decoder *_,bytebuffer *buff,
               uint32_t pos,uint32_t size)
{
	httpdecoder *d = cast(httpdecoder*,_);
	buffer_reader reader;

	if(!d->buff){
		d->buff = bytebuffer_new(size < 1024 ? 1024 : size_of_pow2(size));
	    d->pos  = 0;
    	d->size = 0;	
	}

	if(d->size - d->pos < size && !http_decoder_expand(d,d->size + size)){
		d->status = HTTP_TOOLARGE;
		return;
	}

	buffer_reader_init(&reader,buff,pos);
	buffer_read(&reader,&d->buff->data[d->size],size);
	d->size      += size;
	d->buff->size = d->size;
}

static packet*
http_unpack(decoder *_,int32_t *err){
	httpdecoder *d = cast(httpdecoder*,_);
	packet *ret    = NULL;
	size_t  nparsed,size;
	if(d->status == HTTP_TOOLARGE){
		if(err) *err = EHTTPPARSE;
	}
	else{
		size   = d->size - d->pos;
		nparsed = http_parser_execute(&d->parser,&d->settings,cast(char*,&d->buff->data[d->pos]),size);		
		if(nparsed > 0) d->pos += size;
		if(nparsed != size){
			if(err) *err = EHTTPPARSE;								
		}else if(d->status == HTTP_COMPLETE){		
			d->status   = 0;
			ret         = cast(packet*,d->packet);
			d->packet   = NULL;
			bytebuffer_set(&d->buff,NULL);		
		}
	}
	return ret;
}

void http_decoderdctor(struct decoder *_) 
{
	httpdecoder *d = cast(httpdecoder*,_);
	if(d->packet) packet_del(cast(packet*,d->packet));
}

decoder*
http_decoder_new(uint32_t max_packet_size)
{
	httpdecoder *d     = calloc(1,sizeof(*d));
	d->max_packet_size = max_packet_size < 1024 ? 1024:size_of_pow2(max_packet_size);
	d->unpack 		   = http_unpack;
	d->buff            = bytebuffer_new(1024);
	d->decoder_update  = http_decoder_update;	
	d->settings.on_message_begin = on_message_begin;
	d->settings.on_url = on_url;
	d->settings.on_status = on_status;
	d->settings.on_header_field = on_header_field;
	d->settings.on_header_value = on_header_value;
	d->settings.on_headers_complete = on_headers_complete;
	d->settings.on_body = on_body;
	d->settings.on_message_complete = on_message_complete;
	http_parser_init(&d->parser,HTTP_BOTH);
	return cast(decoder*,d);	
}


void 
decoder_del(decoder *d)
{
	if(d->dctor) d->dctor(d);
	if(d->buff)  refobj_dec(cast(refobj*,d->buff));
	free(d);
}

#ifdef _CHUCKLUA

int32_t 
lua_dgarm_rawpacket_decoder_new(lua_State *L)
{
	lua_pushlightuserdata(L,dgram_raw_decoder_new());
	return 1;
}

int32_t 
lua_conn_rawpacket_decoder_new(lua_State *L)
{
	lua_pushlightuserdata(L,conn_raw_decoder_new());
	return 1;
}

int32_t 
lua_rpacket_decoder_new(lua_State *L)
{
	if(LUA_TNUMBER != lua_type(L,1))
		return luaL_error(L,"arg1 should be number");
	lua_pushlightuserdata(L,rpacket_decoder_new(lua_tonumber(L,1)));
	return 1;
}

int32_t 
lua_http_decoder_new(lua_State *L)
{
	if(LUA_TNUMBER != lua_type(L,1))
		return luaL_error(L,"arg1 should be number");
	lua_pushlightuserdata(L,http_decoder_new(lua_tonumber(L,1)));
	return 1;
}

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)

void 
reg_luadecoder(lua_State *L)
{
	lua_newtable(L);
	SET_FUNCTION(L,"rpacket",lua_rpacket_decoder_new);
	SET_FUNCTION(L,"conn_rawpacket",lua_conn_rawpacket_decoder_new);
	SET_FUNCTION(L,"dgram_rawpacket",lua_dgarm_rawpacket_decoder_new);
	SET_FUNCTION(L,"http",lua_http_decoder_new);
}

#endif