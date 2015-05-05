#include "socket/socket.h"

void close_socket(handle *h)
{
	socket_ *s = (socket_*)h;
	if(!(s->status & SOCKET_CLOSE)){
		s->status |= SOCKET_CLOSE;
		iorequest *req;
		if(s->pending_dctor){
			while((req = (iorequest*)list_pop(&s->pending_send))!=NULL)
				s->pending_dctor(req);
			while((req = (iorequest*)list_pop(&s->pending_recv))!=NULL)
				s->pending_dctor(req);
		}		
		if(!(s->status & SOCKET_INLOOP)){
			close(h->fd);
			if(s->dctor) 
				s->dctor(s);
			else
				free(h);				
		}
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