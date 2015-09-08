#include "packet/chk_rawpacket.h"

static chk_packet *rawpacket_clone(chk_packet*);

static chk_packet_membfunc rawpacket_membf = {
	.construct_write = rawpacket_clone,
	.construct_read  = rawpacket_clone,
	.clone           = rawpacket_clone,
	.dctor           = NULL
};

chk_rawpacket *chk_rawpacket_new(chk_bytechunk *buff,uint32_t spos,uint32_t size) {
	chk_rawpacket           *p = calloc(1,sizeof(*p));
	cast(chk_packet*,p)->membf = rawpacket_membf;
	if(buff && buff->cap - spos >= size) {
		cast(chk_packet*,p)->type       = CHK_RAWPACKET;
		cast(chk_packet*,p)->head       = buff;
		cast(chk_packet*,p)->spos       = spos;
		cast(chk_packet*,p)->len_packet = size;
		chk_bytechunk_retain(buff);
	}
	return p;
}

void *chk_rawpacket_data(chk_rawpacket *p,uint32_t *size) {
	if(size) *size = cast(chk_packet*,p)->len_packet;
	if(!cast(chk_packet*,p)->head) return NULL;
	return cast(void*,cast(chk_packet*,p)->head->data + cast(chk_packet*,p)->spos);
}


static chk_packet *rawpacket_clone(chk_packet *o) {
	chk_rawpacket *p;
	uint32_t spos = cast(chk_packet*,o)->spos;
	uint32_t size = cast(chk_packet*,o)->len_packet;
	p             = chk_rawpacket_new(cast(chk_packet*,o)->head,spos,size);
	return cast(chk_packet*,p);		
}
