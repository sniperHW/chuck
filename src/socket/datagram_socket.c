#include <assert.h>
#include "socket/socket.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"


extern int32_t 
is_read_enable(handle*h);

extern int32_t 
is_write_enable(handle*h);

extern void    
release_socket(socket_ *s);

typedef void(*dgram_callback)(dgram_socket_*,void*,int32_t,int32_t,int32_t);  

static int32_t 
imp_engine_add(engine *e,handle *h,
	           generic_callback callback)
{
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	int32_t ret;
#ifdef _LINUX
	if(0 == (ret = event_add(e,h,EPOLLIN)))
		disable_read(h);	
#elif   _BSD
	if(0 == (ret = event_add(e,h,EVFILT_READ)))
		disable_read(h);
#else
	return -EUNSPPLAT;
#endif
	if(ret == 0){
		easy_noblock(h->fd,1);
		((dgram_socket_*)h)->callback = (dgram_callback)callback;
	}
	return ret;
}

#define pending_recv(S) (((socket_*)S)->pending_recv)
#define status(S)       (((socket_*)S)->status)
#define type(S)         (((socket_*)S)->type)

static void 
process_read(dgram_socket_ *s)
{
	iorequest *req = NULL;
	int32_t bytes = 0;
	struct msghdr _msghdr;
	while((req = (iorequest*)list_pop(&pending_recv(s)))!=NULL){
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
		bytes = TEMP_FAILURE_RETRY(recvmsg(((handle*)s)->fd,&_msghdr,0));	
		if(bytes < 0 && errno == EAGAIN){
			//将请求重新放回到队列
			list_pushback(&pending_recv(s),(listnode*)req);
			break;
		}else{
			s->callback(s,req,bytes,errno,_msghdr.msg_flags);
			if(status(s) & SOCKET_CLOSE)
				return;			
		}
	}	
	if(!list_size(&pending_recv(s))){
		//没有接收请求了,取消EPOLLIN
		disable_read((handle*)s);
	}
}

static void 
on_events(handle *h,int32_t events)
{
	dgram_socket_ *s = (dgram_socket_*)h;
	if(status(s) & SOCKET_CLOSE)
		return;
	do{
		status(s) |= SOCKET_INLOOP;
		if(events & EVENT_READ){
			process_read(s);	
			if(status(s) & SOCKET_CLOSE) 
				break;								
		}				
		status(s) ^= SOCKET_INLOOP;
	}while(0);
	if(status(s) & SOCKET_RELEASE){
		release_socket((socket_*)s);		
	}
}

void    
construct_datagram_socket(dgram_socket_ *s)
{
	((handle*)s)->on_events = on_events;
	((handle*)s)->imp_engine_add = imp_engine_add;
	status(s) = DATAGRAM;	
}		

dgram_socket_*
new_datagram_socket(int32_t fd)
{
	dgram_socket_ *s = calloc(1,sizeof(*s));
	((handle*)s)->fd = fd;
	((handle*)s)->on_events = on_events;
	((handle*)s)->imp_engine_add = imp_engine_add;
	type(s) = DATAGRAM;
	easy_close_on_exec(fd);
	return s;
}

int32_t 
datagram_socket_recv(dgram_socket_ *s,iorequest *req,
	 				 int32_t flag,int32_t *recvflags)
{
	handle *h = (handle*)s;
	if(!h->e)
		return -ENOASSENG;
	else if(status(s) & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	
	errno = 0;
	if(flag == IO_NOW && list_size(&pending_recv(s))){
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
		int32_t bytes = TEMP_FAILURE_RETRY(recvmsg(((handle*)s)->fd,&_msghdr,0));					
		if(*recvflags) *recvflags = _msghdr.msg_flags;	
		if(bytes >= 0)
			return bytes;
		else if(errno != EAGAIN)
			return -errno;
	}
	list_pushback(&pending_recv(s),(listnode*)req);
	if(!is_read_enable(h)) enable_read(h);
	return -EAGAIN;	
}

int32_t 
datagram_socket_send(dgram_socket_ *s,iorequest *req)
{
	handle *h = (handle*)s;
	if(!h->e)
		return -ENOASSENG;
	else if(status(s) & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	
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
	int32_t bytes = TEMP_FAILURE_RETRY(sendmsg(((handle*)s)->fd,&_msghdr,0));		
	if(bytes >= 0)
		return bytes;
	else
		return -errno;	
}