#include "packet/wpacket.h"
#include "packet/rpacket.h"

allocator *g_wpk_allocator = NULL;

static packet*
wpacket_clone(packet*);

packet*
wpacket_makeread(packet*);

packet*
rpacket_makewrite(packet*);


#define INIT_CONSTROUCTOR(p){\
	cast(packet*,p)->construct_write = wpacket_clone;\
	cast(packet*,p)->construct_read  = wpacket_makeread;\
	cast(packet*,p)->clone           = wpacket_clone;\
}


wpacket*
wpacket_new(uint16_t size)
{
	bytebuffer *b;
	wpacket    *w;
	size = size_of_pow2(size);
    if(size < MIN_BUFFER_SIZE) size = MIN_BUFFER_SIZE;
    b = bytebuffer_new(size);
	w = cast(wpacket*,CALLOC(g_wpk_allocator,1,sizeof(*w)));
	cast(packet*,w)->type = WPACKET;
	cast(packet*,w)->head = b;
	buffer_writer_init(&w->writer,b,sizeof(*w->len));
	w->len = cast(TYPE_HEAD*,b->data);
	cast(packet*,w)->len_packet = SIZE_HEAD;
	cast(packet*,w)->head->size = SIZE_HEAD;
	INIT_CONSTROUCTOR(w);	
	return w;
}

static packet*
wpacket_clone(packet *p)
{
	if(p->type == WPACKET){
		wpacket *w = cast(wpacket*,CALLOC(g_wpk_allocator,1,sizeof(*w)));
		cast(packet*,w)->type = WPACKET;
		cast(packet*,w)->head = p->head;
		refobj_inc(cast(refobj*,p->head));		
		cast(packet*,w)->spos = p->spos;
		cast(packet*,w)->len_packet = p->len_packet;		
		INIT_CONSTROUCTOR(w);
		return cast(packet*,w);		
	}else
		return NULL;
}

packet*
rpacket_makewrite(packet *p)
{
	if(p->type == RPACKET){
		wpacket *w = cast(wpacket*,CALLOC(g_wpk_allocator,1,sizeof(*w)));	
		cast(packet*,w)->type = WPACKET;
		cast(packet*,w)->head = p->head;
		refobj_inc(cast(refobj*,p->head));		
		cast(packet*,w)->spos = p->spos;
		cast(packet*,w)->len_packet = p->len_packet;		
		INIT_CONSTROUCTOR(w);		
		return cast(packet*,w);		
	}else
		return NULL;
}