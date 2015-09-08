#include "packet/chk_rpacket.h"
#include "packet/chk_wpacket.h"

static chk_packet *rpacket_clone(chk_packet*);
static void        rpacket_dctor(chk_packet*);
chk_packet        *rpacket_makewrite(chk_packet*);

static chk_packet_membfunc rpacket_membf = {
	.construct_write = rpacket_makewrite,
	.construct_read  = rpacket_clone,
	.clone           = rpacket_clone,
	.dctor           = rpacket_dctor
};


//用于读取长度字段
static inline uint32_t rpacket_readLen(chk_rpacket *r) {
#if CHK_LEN_TYPE == uint16_t   
    return chk_rpacket_readU16(r);
#else
    return chk_rpacket_readU32(r);
#endif  
}

//用于peek长度字段
static inline uint32_t rpacket_peekLen(chk_rpacket *r) {
#if CHK_LEN_TYPE == uint16_t   
    return chk_rpacket_peekU16(r);
#else
    return chk_rpacket_peekU32(r);
#endif  
}


chk_rpacket *chk_rpacket_new(chk_bytechunk *b,uint32_t start_pos) {
	chk_rpacket *p   = calloc(1,sizeof(*p));
	uint32_t    size = sizeof(CHK_LEN_TYPE);
	cast(chk_packet*,p)->membf = rpacket_membf;
	if(b) {
		chk_bytechunk_retain(b);		
		cast(chk_packet*,p)->head = b;
		cast(chk_packet*,p)->spos = start_pos;
		p->readbuf = chk_bytechunk_read(b,cast(char*,&p->data_remain),&p->readpos,&size);
#if CHK_LEN_TYPE == uint16_t 		
		p->data_remain = chk_ntoh16(p->data_remain); 
#else
		p->data_remain = chk_ntoh32(p->data_remain); 
#endif
		cast(chk_packet*,p)->len_packet = p->data_remain + size;
	}
	return p;
}

static chk_packet *rpacket_clone(chk_packet *p) {
	chk_rpacket *r;
	if(p->type == CHK_RPACKET) {
		r = chk_rpacket_new(p->head,p->spos);
		return cast(chk_packet*,r);
	}
	return NULL;
}

chk_packet *wpacket_makeread(chk_packet *p) {
	chk_rpacket *r;
	if(p->type == CHK_WPACKET) {
		r = chk_rpacket_new(p->head,p->spos);	
		return cast(chk_packet*,r);		
	}
	return NULL;
}

void *chk_rpacket_readBin(chk_rpacket *r,uint32_t *len) {
	void    *ret;
	uint32_t size = rpacket_readLen(r);
	if(r->data_remain < size) return NULL;
	*len = size;
	if(r->readbuf->cap - r->readpos >= size) {
		ret = cast(void*,r->readbuf->data + r->readpos);
		r->readpos     += size;
        if(r->readpos >= r->readbuf->cap) {
            r->readpos = 0;
            r->readbuf = r->readbuf->next;
        }		
	}else {
		//数据跨buffer
		if(!r->binbuf) {
			r->binbuf = chk_bytechunk_new(NULL,r->data_remain);
			r->binpos = 0;
		}
		ret = cast(void*,r->binbuf->data + r->binpos);
		r->readbuf = chk_bytechunk_read(r->readbuf,r->binbuf->data,&r->readpos,&size);
		r->binpos  += size;
	}
    r->data_remain -= size;	
	return ret;
}

const char *chk_rpacket_readCStr(chk_rpacket *r)
{
	uint32_t  len;	
	char *str =  cast(char*,chk_rpacket_readBin(r,&len));
	if(str && str[len-1] == '\0') return str;	
	return NULL;
}

static void rpacket_dctor(chk_packet *_) {
	chk_rpacket *r = cast(chk_rpacket*,_);
	if(r->binbuf) chk_bytechunk_retain(r->binbuf);
}
