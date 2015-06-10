#include "socket/socket.h"
#include "engine/engine.h"

void 
release_socket(socket_ *s)
{
	iorequest *req;
	list      *l;
	close(s->fd);
	if(s->pending_dctor){
		l = &s->pending_recv;
		while((req = cast(iorequest*,list_pop(l)))!=NULL)
			s->pending_dctor(req);
		if(s->type == STREAM){
			l = &cast(stream_socket_*,s)->pending_send;
			while((req = cast(iorequest*,list_pop(l)))!=NULL)
				s->pending_dctor(req);
		}	
	}	
	if(s->dctor) 
		s->dctor(s);
	else
		free(s);
}

void 
close_socket(socket_ *s)
{
	if(s->status & SOCKET_CLOSE)
		return;
	s->status |= SOCKET_CLOSE;
	engine_remove(cast(handle*,s));			
	if(!(s->status & SOCKET_INLOOP)){
		release_socket(s);
	}
}

int32_t 
is_read_enable(handle*h)
{
#ifdef _LINUX
	return h->events & EPOLLIN;
#elif   _BSD
	return cast(int32_t,h->set_read);
#endif
	return 0;
}

int32_t 
is_write_enable(handle*h)
{
#ifdef _LINUX
	return h->events & EPOLLOUT;
#elif   _BSD
	return cast(int32_t,h->set_write);
#endif
	return 0;	
}