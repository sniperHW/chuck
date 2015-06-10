#include "comm.h"
#include "packet/packet.h"
#include "packet/wpacket.h"
#include "packet/rpacket.h"

void 
packet_del(packet *p)
{
	if(p->type == RPACKET && cast(rpacket*,p)->binbuf)
		refobj_dec(cast(refobj*,cast(rpacket*,p)->binbuf));
	refobj_dec(cast(refobj*,p->head));
	free(p);
}