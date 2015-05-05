#include "packet/rawpacket.h"

allocator *g_rawpk_allocator = NULL;

static packet *rawpacket_clone(packet*);

#define INIT_CONSTROUCTOR(p){\
	((packet*)p)->construct_write = rawpacket_clone;\
	((packet*)p)->construct_read = rawpacket_clone;\
	((packet*)p)->clone 		 = rawpacket_clone;\
}


rawpacket *rawpacket_new(uint32_t size){
	size = size_of_pow2(size);
    if(size < MIN_BUFFER_SIZE) size = MIN_BUFFER_SIZE;
    bytebuffer *b = bytebuffer_new(size);
	rawpacket *raw = (rawpacket*)CALLOC(g_rawpk_allocator,1,sizeof(*raw));
	((packet*)raw)->type = RAWPACKET;
	((packet*)raw)->head = b;
	buffer_writer_init(&raw->writer,b,0);
	INIT_CONSTROUCTOR(raw);
	return raw;
}

//will add reference count of b
rawpacket *rawpacket_new_by_buffer(bytebuffer *b,uint32_t spos){
	rawpacket *raw = (rawpacket*)CALLOC(g_rawpk_allocator,1,sizeof(*raw));
	((packet*)raw)->type = RAWPACKET;
	((packet*)raw)->head = b;
	((packet*)raw)->spos = spos;
	refobj_inc((refobj*)b);
	((packet*)raw)->len_packet = b->size - spos;
	buffer_writer_init(&raw->writer,b,b->size);
	INIT_CONSTROUCTOR(raw);
	return raw;		
}

static packet *rawpacket_clone(packet *p){
	rawpacket *raw = (rawpacket*)CALLOC(g_rawpk_allocator,1,sizeof(*raw));
	((packet*)raw)->type = RAWPACKET;
	((packet*)raw)->head = p->head;
	((packet*)raw)->spos = p->spos;
	refobj_inc((refobj*)p->head);
	((packet*)raw)->len_packet = p->len_packet;
	//buffer_writer_init(&raw->writer,p->head,p->len_packet);
	INIT_CONSTROUCTOR(raw);
	return (packet*)raw;	
}