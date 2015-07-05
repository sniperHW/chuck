#include "comm.h"
#include "packet/packet.h"
#include "packet/wpacket.h"
#include "packet/rpacket.h"
#include "packet/httppacket.h"
#include "packet/rawpacket.h"
#include <assert.h>

void packet_del(packet *p)
{
	if(p->dctor) p->dctor(p);
	if(p->type == RPACKET && cast(rpacket*,p)->binbuf)
		refobj_dec(cast(refobj*,cast(rpacket*,p)->binbuf));
	refobj_dec(cast(refobj*,p->head));
	switch(p->type){
		case WPACKET:FREE(g_wpk_allocator,p);break;
		case RPACKET:FREE(g_rpk_allocator,p);break;
		case RAWPACKET:FREE(g_rawpk_allocator,p);break;
		case HTTPPACKET:FREE(g_http_allocator,p);break;
		default:assert(0);
	}
}