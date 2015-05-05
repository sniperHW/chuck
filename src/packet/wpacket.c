#include "packet/wpacket.h"
#include "packet/rpacket.h"

allocator *g_wpk_allocator = NULL;

static packet *wpacket_clone(packet*);
packet        *wpacket_makeread(packet*);
packet        *rpacket_makewrite(packet*);


#define INIT_CONSTROUCTOR(p){\
	((packet*)p)->construct_write = wpacket_clone;\
	((packet*)p)->construct_read = wpacket_makeread;\
	((packet*)p)->clone = wpacket_clone;\
}


wpacket *wpacket_new(uint16_t size){
	size = size_of_pow2(size);
    if(size < MIN_BUFFER_SIZE) size = MIN_BUFFER_SIZE;
    bytebuffer *b = bytebuffer_new(size);

	wpacket *w = (wpacket*)CALLOC(g_wpk_allocator,1,sizeof(*w));
	((packet*)w)->type = WPACKET;
	((packet*)w)->head = b;
	buffer_writer_init(&w->writer,b,sizeof(*w->len));
	w->len = (TYPE_HEAD*)b->data;
	((packet*)w)->len_packet = SIZE_HEAD;
	((packet*)w)->head->size = SIZE_HEAD;
	INIT_CONSTROUCTOR(w);	
	return w;
}

static packet *wpacket_clone(packet *p){
	if(p->type == WPACKET){
		wpacket *w = (wpacket*)CALLOC(g_wpk_allocator,1,sizeof(*w));
		((packet*)w)->type = WPACKET;
		((packet*)w)->head = p->head;
		refobj_inc((refobj*)p->head);		
		((packet*)w)->spos = p->spos;
		((packet*)w)->len_packet = p->len_packet;		
		INIT_CONSTROUCTOR(w);
		return (packet*)w;		
	}else
		return NULL;
}

packet *rpacket_makewrite(packet *p){
	if(p->type == RPACKET){
		wpacket *w = (wpacket*)CALLOC(g_wpk_allocator,1,sizeof(*w));	
		((packet*)w)->type = WPACKET;
		((packet*)w)->head = p->head;
		refobj_inc((refobj*)p->head);		
		((packet*)w)->spos = p->spos;
		((packet*)w)->len_packet = p->len_packet;		
		INIT_CONSTROUCTOR(w);		
		return (packet*)w;		
	}else
		return NULL;
}