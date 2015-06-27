#include "packet/rawpacket.h"

allocator *g_rawpk_allocator = NULL;

static packet *rawpacket_clone(packet*);

#define INIT_CONSTROUCTOR(p){\
	cast(packet*,p)->construct_write = rawpacket_clone;\
	cast(packet*,p)->construct_read  = rawpacket_clone;\
	cast(packet*,p)->clone 		     = rawpacket_clone;\
}


rawpacket *rawpacket_new(uint32_t size)
{
	bytebuffer *b;
	rawpacket  *raw;
	size = size_of_pow2(size);
    if(size < MIN_BUFFER_SIZE) size = MIN_BUFFER_SIZE;
    b = bytebuffer_new(size);
	raw = cast(rawpacket*,CALLOC(g_rawpk_allocator,1,sizeof(*raw)));
	cast(packet*,raw)->type = RAWPACKET;
	cast(packet*,raw)->head = b;
	buffer_writer_init(&raw->writer,b,0);
	INIT_CONSTROUCTOR(raw);
	return raw;
}

//will add reference count of b
rawpacket *rawpacket_new_by_buffer(bytebuffer *b,uint32_t spos)
{
	rawpacket *raw = cast(rawpacket*,CALLOC(g_rawpk_allocator,1,sizeof(*raw)));
	cast(packet*,raw)->type = RAWPACKET;
	cast(packet*,raw)->head = b;
	cast(packet*,raw)->spos = spos;
	refobj_inc(cast(refobj*,b));
	cast(packet*,raw)->len_packet = b->size - spos;
	buffer_writer_init(&raw->writer,b,b->size);
	INIT_CONSTROUCTOR(raw);
	return raw;		
}

static packet *rawpacket_clone(packet *p)
{
	rawpacket *raw = cast(rawpacket*,CALLOC(g_rawpk_allocator,1,sizeof(*raw)));
	cast(packet*,raw)->type = RAWPACKET;
	cast(packet*,raw)->head = p->head;
	cast(packet*,raw)->spos = p->spos;
	refobj_inc(cast(refobj*,p->head));
	cast(packet*,raw)->len_packet = p->len_packet;
	//buffer_writer_init(&raw->writer,p->head,p->len_packet);
	INIT_CONSTROUCTOR(raw);
	return cast(packet*,raw);	
}