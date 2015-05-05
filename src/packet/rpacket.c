#include "packet/rpacket.h"
#include "packet/wpacket.h"

allocator *g_rpk_allocator = NULL;

static packet  *rpacket_clone(packet*);
packet  	   *wpacket_makeread(packet*);
packet         *rpacket_makewrite(packet*);

#define INIT_CONSTROUCTOR(p){\
	((packet*)p)->construct_write = rpacket_makewrite;\
	((packet*)p)->construct_read = rpacket_clone;\
	((packet*)p)->clone = rpacket_clone;\
}

rpacket *rpacket_new(bytebuffer *b,uint32_t start_pos){
	rpacket *r = (rpacket*)CALLOC(g_rpk_allocator,1,sizeof(*r));
	((packet*)r)->type = RPACKET;
	if(b){
		((packet*)r)->head = b;
		((packet*)r)->spos = start_pos;
		refobj_inc((refobj*)b);
		buffer_reader_init(&r->reader,b,start_pos);
		buffer_read(&r->reader,&r->data_remain,sizeof(r->data_remain));		
		((packet*)r)->len_packet = r->data_remain + SIZE_HEAD;
	}
	INIT_CONSTROUCTOR(r);
	return r;
}


const void *rpacket_read_binary(rpacket *r,uint16_t *len){
	void *addr = 0;
	uint16_t size = rpacket_read_uint16(r);
	if(size == 0 || size > r->data_remain) return NULL;
	if(len) *len = size;
	if(reader_check_size(&r->reader,size)){
		addr = &r->reader.cur->data[r->reader.pos];
		r->reader.pos += size;
		r->data_remain -= size;
		if(r->data_remain && r->reader.pos >= r->reader.cur->size)
			buffer_reader_init(&r->reader,r->reader.cur->next,0);
	}else{
		if(!r->binbuf){
			r->binbuf = bytebuffer_new(((packet*)r)->len_packet);
			r->binpos = 0;
		}
		addr = r->binbuf->data + r->binpos;
		while(size)
		{
			uint32_t copy_size = r->reader.cur->size - r->reader.pos;
			copy_size = copy_size >= size ? size:copy_size;
			memcpy(r->binbuf->data + r->binpos,r->reader.cur->data + r->reader.pos,copy_size);
			size -= copy_size;
			r->reader.pos += copy_size;
			r->data_remain -= copy_size;
			r->binpos += copy_size;
			if(r->data_remain && r->reader.pos >= r->reader.cur->size)
				buffer_reader_init(&r->reader,r->reader.cur->next,0);
		}		
	}
	return addr;
}

const char *rpacket_read_string(rpacket *r){
	return rpacket_read_binary(r,NULL);
}

static packet  *rpacket_clone(packet *p){
	rpacket *ori = (rpacket*)p;
	if(p->type == RPACKET){
		rpacket *r = rpacket_new(NULL,0);
		((packet*)r)->head = p->head;
		refobj_inc((refobj*)p->head);
		((packet*)r)->spos = p->spos;
		buffer_reader_init(&r->reader,ori->reader.cur,ori->reader.pos);
		((packet*)r)->len_packet =  p->len_packet;
		r->data_remain = p->len_packet - SIZE_HEAD;
		INIT_CONSTROUCTOR(r);	
		return (packet*)r;
	}
	return NULL;
}


packet  *wpacket_makeread(packet *p){
	if(p->type == WPACKET){
		rpacket *r = rpacket_new(NULL,0);
		((packet*)r)->head = p->head;
		refobj_inc((refobj*)p->head);
		((packet*)r)->spos = p->spos;
		buffer_reader_init(&r->reader,p->head,p->spos + SIZE_HEAD);
		((packet*)r)->len_packet = p->len_packet;
		r->data_remain = p->len_packet - SIZE_HEAD;
		INIT_CONSTROUCTOR(r);		
		return (packet*)r;		
	}
	return NULL;
}