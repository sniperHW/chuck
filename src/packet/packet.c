#include "packet/packet.h"
#include "packet/wpacket.h"
#include "packet/rpacket.h"

void packet_del(packet *p){
	if(p->type == RPACKET && ((rpacket*)p)->binbuf)
		refobj_dec((refobj*)((rpacket*)p)->binbuf);
	refobj_dec((refobj*)p->head);
	free(p);
}