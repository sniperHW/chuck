#include <assert.h>
#include "socket/socket.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"

extern void    
release_socket(socket_ *s);

typedef void(*dgram_callback)(dgram_socket_*,void*,int32_t,int32_t,int32_t);  

static int32_t 
imp_engine_add(engine *e,handle *h,
	           generic_callback callback)
{
	int32_t ret;
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	ret = event_add(e,h,EVENT_READ) || event_add(e,h,EVENT_WRITE);
	if(ret == 0){
		easy_noblock(h->fd,1);
		cast(dgram_socket_*,h)->callback = cast(dgram_callback,callback);
	}
	return ret;
}


static void 
process_read(dgram_socket_ *s)
{
	iorequest *req = NULL;
	int32_t bytes = 0;
	struct msghdr _msghdr;
	s->status |= SOCKET_READABLE;
	if((req = cast(iorequest*,list_pop(&s->pending_recv)))!=NULL){
		errno = 0;
		_msghdr = (struct msghdr){
			.msg_name = &req->addr,
			.msg_namelen = sizeof(req->addr),
			.msg_iov = req->iovec,
			.msg_iovlen = req->iovec_count,
			.msg_flags = 0,
			.msg_control = NULL,
			.msg_controllen = 0
		};
		bytes = TEMP_FAILURE_RETRY(recvmsg(s->fd,&_msghdr,0));	
		s->callback(s,req,bytes,errno,_msghdr.msg_flags);
		if(s->status & SOCKET_CLOSE)
			return;		
	}	
	if(s->e && !list_size(&s->pending_recv)){
		//没有接收请求了,取消EPOLLIN
		disable_read(cast(handle*,s));
	}
}

static void 
on_events(handle *h,int32_t events)
{
	dgram_socket_ *s = cast(dgram_socket_*,h);
	if(!s->e || ((s->status & SOCKET_CLOSE)))
		return;
	if(events == EVENT_ECLOSE){
		s->callback(s,NULL,-1,EENGCLOSE,0);
		return;
	}	
	do{
		s->status |= SOCKET_INLOOP;
		if(events & EVENT_READ){
			process_read(s);	
			if(s->status & SOCKET_CLOSE) 
				break;								
		}				
		s->status ^= SOCKET_INLOOP;
	}while(0);
	if(s->status & SOCKET_CLOSE){
		release_socket(cast(socket_*,s));		
	}
}

void    
datagram_socket_init(dgram_socket_ *s,int32_t fd)
{
	s->fd = fd;
	s->on_events = on_events;
	s->imp_engine_add = imp_engine_add;
	s->type = DATAGRAM;
	easy_close_on_exec(fd);
}		

dgram_socket_*
new_datagram_socket(int32_t fd)
{
	dgram_socket_ *s = calloc(1,sizeof(*s));
	datagram_socket_init(s,fd);
	return s;
}

int32_t 
datagram_socket_recv(dgram_socket_ *s,iorequest *req,
	 				 int32_t flag,int32_t *recvflags)
{
	int32_t bytes;
	handle *h = cast(handle*,s);

	if(s->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	else if(!h->e)
		return -ENOASSENG;
	errno = 0;
	if(s->status & SOCKET_READABLE && 
	   flag == IO_NOW && 
	   !list_size(&s->pending_recv))
	{
		struct msghdr _msghdr = {
			.msg_name = &req->addr,
			.msg_namelen = sizeof(req->addr),
			.msg_iov = req->iovec,
			.msg_iovlen = req->iovec_count,
			.msg_flags = 0,
			.msg_control = NULL,
			.msg_controllen = 0
		};
		if(*recvflags) *recvflags = 0;		
		bytes = TEMP_FAILURE_RETRY(recvmsg(s->fd,&_msghdr,0));					
		if(*recvflags) *recvflags = _msghdr.msg_flags;	
		if(bytes >= 0)
			return bytes;
		else if(errno != EAGAIN)
			return -errno;
	}
	s->status ^= SOCKET_READABLE;
	list_pushback(&s->pending_recv,cast(listnode*,req));
	if(!is_read_enable(h)) enable_read(h);
	return -EAGAIN;	
}

int32_t 
datagram_socket_send(dgram_socket_ *s,iorequest *req)
{
	int32_t bytes;
	handle *h = cast(handle*,s);

	if(s->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	else if(!h->e)
		return -ENOASSENG;	
	errno = 0;
	struct msghdr _msghdr = {
		.msg_name = &req->addr,
		.msg_namelen = sizeof(req->addr),
		.msg_iov = req->iovec,
		.msg_iovlen = req->iovec_count,
		.msg_flags = 0,
		.msg_control = NULL,
		.msg_controllen = 0
	};		
	bytes = TEMP_FAILURE_RETRY(sendmsg(s->fd,&_msghdr,0));		
	if(bytes >= 0)
		return bytes;
	else
		return -errno;	
}