#include "packet/rpacket.h"
#include "packet/wpacket.h"

static packet *rpacket_clone(packet*);

packet *wpacket_makeread(packet*);

packet *rpacket_makewrite(packet*);

#define INIT_CONSTROUCTOR(p){\
	cast(packet*,p)->construct_write = rpacket_makewrite;\
	cast(packet*,p)->construct_read  = rpacket_clone;\
	cast(packet*,p)->clone           = rpacket_clone;\
}

rpacket *rpacket_new(bytebuffer *b,uint32_t start_pos)
{
	rpacket *r = calloc(1,sizeof(*r));
	cast(packet*,r)->type = RPACKET;
	if(b){
		cast(packet*,r)->head = b;
		cast(packet*,r)->spos = start_pos;
		refobj_inc(cast(refobj*,b));
		buffer_reader_init(&r->reader,b,start_pos);
		buffer_read(&r->reader,&r->data_remain,sizeof(r->data_remain));
		r->data_remain = ntoh(r->data_remain);		
		cast(packet*,r)->len_packet = r->data_remain + SIZE_HEAD;
	}
	INIT_CONSTROUCTOR(r);
	return r;
}


const void *rpacket_read_binary(rpacket *r,TYPE_HEAD *len)
{
	void    *addr = 0;
	uint32_t copy_size;
#if HEAD_SIZE == uint16_t	
	TYPE_HEAD size = rpacket_read_uint16(r);
#else
	TYPE_HEAD size = rpacket_read_uint32(r)
#endif	
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
			r->binbuf = bytebuffer_new(cast(packet*,r)->len_packet);
			r->binpos = 0;
		}
		addr = r->binbuf->data + r->binpos;
		while(size)
		{
			copy_size = r->reader.cur->size - r->reader.pos;
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

const char *rpacket_read_string(rpacket *r)
{
	TYPE_HEAD len;	
	char *str =  (char*)rpacket_read_binary(r,&len);
	if(str && str[len-1] == '\0')
		return str;	
	return NULL;
}

static packet *rpacket_clone(packet *p)
{
	rpacket *r,*ori;
	ori = (rpacket*)p;
	if(p->type == RPACKET){
		r = rpacket_new(NULL,0);
		cast(packet*,r)->head = p->head;
		refobj_inc(cast(refobj*,p->head));
		cast(packet*,r)->spos = p->spos;
		buffer_reader_init(&r->reader,ori->reader.cur,ori->reader.pos);
		cast(packet*,r)->len_packet =  p->len_packet;
		r->data_remain = p->len_packet - SIZE_HEAD;
		INIT_CONSTROUCTOR(r);	
		return cast(packet*,r);
	}
	return NULL;
}


packet *wpacket_makeread(packet *p)
{
	rpacket *r;
	if(p->type == WPACKET){
		r = rpacket_new(NULL,0);
		cast(packet*,r)->head = p->head;
		refobj_inc(cast(refobj*,p->head));
		cast(packet*,r)->spos = p->spos;
		buffer_reader_init(&r->reader,p->head,p->spos + SIZE_HEAD);
		cast(packet*,r)->len_packet = p->len_packet;
		r->data_remain = p->len_packet - SIZE_HEAD;
		INIT_CONSTROUCTOR(r);		
		return cast(packet*,r);		
	}
	return NULL;
}