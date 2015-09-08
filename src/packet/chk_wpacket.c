#include "packet/chk_wpacket.h"
#include "packet/chk_rpacket.h"

static chk_packet *wpacket_clone(chk_packet*);
chk_packet        *wpacket_makeread(chk_packet *p);

static chk_packet_membfunc wpacket_membf = {
	.construct_write = wpacket_clone,
	.construct_read  = wpacket_makeread,
	.clone           = wpacket_clone,
	.dctor           = NULL
};

chk_wpacket *chk_wpacket_new(uint32_t size) {
	chk_bytechunk *b;
	chk_wpacket   *w;
	b = chk_bytechunk_new(NULL,size);
	w = calloc(1,sizeof(*w));
	w->len = cast(CHK_LEN_TYPE*,b->data);
	cast(chk_packet*,w)->type = CHK_WPACKET;
	cast(chk_packet*,w)->head = b;
	*w->len = 0;
	cast(chk_packet*,w)->len_packet = sizeof(*w->len);
	cast(chk_packet*,w)->membf = wpacket_membf;
	w->wpos = sizeof(*w->len);
	w->wbuf = b;
	return w;
}

static chk_packet *wpacket_clone(chk_packet *o) {
	chk_wpacket   *w = calloc(1,sizeof(*w));
	cast(chk_packet*,w)->type = CHK_WPACKET;
	cast(chk_packet*,w)->head = o->head;
	chk_bytechunk_retain(o->head);
	cast(chk_packet*,w)->spos = o->spos;
	cast(chk_packet*,w)->len_packet = o->len_packet;
	cast(chk_packet*,w)->membf = wpacket_membf;
	w->wpos = cast(chk_wpacket*,o)->wpos;
	return cast(chk_packet*,w);
}

chk_packet *rpacket_makewrite(chk_packet *o) {
	chk_wpacket   *w = NULL;
	if(o->type == CHK_RPACKET) {
		w = calloc(1,sizeof(*w));
		cast(chk_packet*,w)->type = CHK_WPACKET;
		cast(chk_packet*,w)->head = o->head;
		chk_bytechunk_retain(o->head);
		cast(chk_packet*,w)->spos = o->spos;
		cast(chk_packet*,w)->len_packet = o->len_packet;		
		cast(chk_packet*,w)->membf = wpacket_membf;
	} 
	return cast(chk_packet*,w);
}