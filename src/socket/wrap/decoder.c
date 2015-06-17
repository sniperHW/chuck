#include "socket/wrap/decoder.h"
#include "packet/rpacket.h"
#include "packet/rawpacket.h"

static inline void 
_decoder_init(decoder *d,bytebuffer *buff,uint32_t pos)
{
    d->buff = buff;
    refobj_inc((refobj*)buff);
    d->pos  = pos;
    d->size = 0;
}

static inline void 
_decoder_update(decoder *d,bytebuffer *buff,
               uint32_t pos,uint32_t size)
{
    if(!d->buff)
        _decoder_init(d,buff,pos);
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
	d->decoder_init = _decoder_init;
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
	d->decoder_init = _decoder_init;
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
	d->decoder_init = _decoder_init;
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
	decoder->packet      = httppacket_new();
	return 0;
}

static int 
on_url(http_parser *parser, const char *at, size_t length)
{	
	httpdecoder *decoder = cast2httpdecoder(parser);
	httppacket_onurl(decoder->packet,at,length);
	return 0;
}

static int 
on_status(http_parser *parser, const char *at, size_t length)
{
	httpdecoder *decoder = cast2httpdecoder(parser);
	httppacket_onstatus(decoder->packet,at,length);	
	return 0;			
}

static int 
on_header_field(http_parser *parser, const char *at, size_t length)
{
	httpdecoder *decoder = cast2httpdecoder(parser);
	httppacket_on_header_field(decoder->packet,at,length);	
	return 0;			
}

static int 
on_header_value(http_parser *parser, const char *at, size_t length)
{
	httpdecoder *decoder = cast2httpdecoder(parser);
	httppacket_on_header_value(decoder->packet,at,length);
	return 0;			
}

static int 
on_headers_complete(http_parser *parser)
{	
	return 0;		
}

static int 
on_body(http_parser *parser, const char *at, size_t length)
{
	httpdecoder *decoder = cast2httpdecoder(parser);
	if(length > decoder->max_packet_size)
		return -1;
	httppacket_on_body(decoder->packet,at,length);	
	return 0;			
}

static int 
on_message_complete(http_parser *parser)
{	
	httpdecoder *decoder = cast2httpdecoder(parser);
	decoder->complete = 1;
	decoder->pos      = 0;
	decoder->size     = 0;
	return 0;							
}


static inline void 
http_decoder_init(decoder *d,bytebuffer *buff,uint32_t pos)
{
    d->pos  = 0;
    d->size = 0;
}

static inline void 
http_decoder_update(decoder *_,bytebuffer *buff,
               uint32_t pos,uint32_t size)
{
	httpdecoder *d = cast(httpdecoder*,_);
	buffer_reader reader;
	uint32_t space = d->cap - d->pos;
	uint32_t newcap;
	char    *tmp;
	buffer_reader_init(&reader,buff,pos);
	if(space < size){
		newcap = d->cap*2;
		tmp = realloc(d->databuffer,newcap);	
		if(!(tmp = realloc(d->databuffer,newcap))) return;
		if(tmp != d->databuffer) d->databuffer = tmp;
		d->cap = newcap;
	}

	buffer_read(&reader,&d->databuffer[d->pos],size);
	d->size       += size;
	d->pos        += pos;
}

static packet*
http_unpack(decoder *_,int32_t *err){
	httpdecoder *d = cast(httpdecoder*,_);
	packet *ret = NULL;
	size_t  size = d->size - d->pos;
	size_t nparsed = http_parser_execute(&d->parser,&d->settings,cast(char*,&d->databuffer[d->pos]),size);		
	if(nparsed != size){
		if(err) *err = EPKTOOLARGE;										
	}else if(d->complete){
		d->complete = 0;
		ret         = cast(packet*,d->packet);
		d->packet   = NULL;
	}
	return ret;
}

void
http_decoder_dctor(decoder *_)
{
	httpdecoder *d = cast(httpdecoder*,_);
	free(d->databuffer);
}	

decoder*
http_decoder_new(uint32_t max_body_size)
{
	httpdecoder *d     = calloc(1,sizeof(*d));
	d->max_packet_size = max_body_size;
	d->databuffer      = malloc(max_body_size);
	d->cap             = max_body_size;
	d->unpack 		   = http_unpack;
	d->dctor           = http_decoder_dctor;
	d->decoder_init    = http_decoder_init;
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
	if(d->buff) bytebuffer_set(&d->buff,NULL);
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
	SET_FUNCTION(L,"dgram_rawpacket",lua_dgram_rawpacket_decoder_new);
}

#endif