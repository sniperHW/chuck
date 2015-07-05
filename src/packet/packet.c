#include "comm.h"
#include "packet/packet.h"
#include "packet/rpacket.h"
#include <assert.h>

void packet_del(packet *p)
{
	if(p->dctor) p->dctor(p);
	if(p->type == RPACKET && cast(rpacket*,p)->binbuf)
		refobj_dec(cast(refobj*,cast(rpacket*,p)->binbuf));
	refobj_dec(cast(refobj*,p->head));
	free(p);
}