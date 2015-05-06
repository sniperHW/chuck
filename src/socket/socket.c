#include "socket/socket.h"
#include "engine/engine.h"

void release_socket(socket_ *s){
	close(((handle*)s)->fd);
	iorequest *req;
	if(s->pending_dctor){
		while((req = (iorequest*)list_pop(&s->pending_send))!=NULL)
			s->pending_dctor(req);
		while((req = (iorequest*)list_pop(&s->pending_recv))!=NULL)
			s->pending_dctor(req);
	}	
	if(s->dctor) 
		s->dctor(s);
	else
		free(s);
}

void close_socket(handle *h)
{
	socket_ *s = (socket_*)h;
	if(s->status & SOCKET_RELEASE)
		return;
	s->status |= (SOCKET_CLOSE | SOCKET_RELEASE);
	engine_remove(h);			
	if(!(s->status & SOCKET_INLOOP)){
		release_socket(s);
	}
}

int32_t is_read_enable(handle*h){
#ifdef _LINUX
	return h->events & EPOLLIN;
#elif   _BSD
	return (int32_t)h->set_read;
#endif
	return 0;
}

int32_t is_write_enable(handle*h){
#ifdef _LINUX
	return h->events & EPOLLOUT;
#elif   _BSD
	return (int32_t)h->set_write;
#endif
	return 0;	
}