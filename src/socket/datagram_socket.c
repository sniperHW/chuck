#include <assert.h>
#include "socket/socket.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"


extern int32_t is_read_enable(handle*h);
extern int32_t is_write_enable(handle*h);

static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback){
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
		((socket_*)h)->datagram_callback = (void(*)(handle*,void*,int32_t,int32_t,int32_t))callback;
	}
	return ret;
}

static void process_read(socket_ *s){
	iorequest *req = NULL;
	int32_t bytes_transfer = 0;
	struct msghdr _msghdr;
	while((req = (iorequest*)list_pop(&s->pending_recv))!=NULL){
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
		bytes_transfer = TEMP_FAILURE_RETRY(recvmsg(((handle*)s)->fd,&_msghdr,0));	
		if(bytes_transfer < 0 && errno == EAGAIN){
			//将请求重新放回到队列
			list_pushback(&s->pending_recv,(listnode*)req);
			break;
		}else{
			s->datagram_callback((handle*)s,req,bytes_transfer,errno,_msghdr.msg_flags);
			if(s->status & SOCKET_CLOSE)
				return;			
		}
	}	
	if(!list_size(&s->pending_recv)){
		//没有接收请求了,取消EPOLLIN
		disable_read((handle*)s);
	}
}

/*
static void process_write(socket_ *s){
	iorequest *req = NULL;
	int32_t bytes_transfer = 0;
	struct msghdr _msghdr;
	while((req = (iorequest*)list_pop(&s->pending_send))!=NULL){
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
		bytes_transfer = TEMP_FAILURE_RETRY(sendmsg(((handle*)s)->fd,&_msghdr,0));	
		s->datagram_callback((handle*)s,req,bytes_transfer,errno,0);
		if(s->status & SOCKET_CLOSE)
			return;			
	}	
	if(!list_size(&s->pending_send)){
		disable_write((handle*)s);
	}			
}*/


static void on_events(handle *h,int32_t events){
	socket_ *s = (socket_*)h;
	if(s->status & SOCKET_CLOSE)
		return;
	do{
		s->status |= SOCKET_INLOOP;
		if(events & EVENT_READ){
			process_read(s);	
			if(s->status & SOCKET_CLOSE) 
				break;								
		}		
		//if(events & EVENT_WRITE)
		//	process_write(s);			
		s->status ^= SOCKET_INLOOP;
	}while(0);
	if(s->status & SOCKET_CLOSE){
		close(h->fd);		
		if(s->dctor) 
			s->dctor(s);
		else		
			free(h);		
	}
}

void    construct_datagram_socket(socket_ *s){
	((handle*)s)->on_events = on_events;
	((handle*)s)->imp_engine_add = imp_engine_add;
	s->status = SOCKET_DATAGRAM;	
}		

handle *new_datagram_socket(int32_t fd){
	socket_ *s = calloc(1,sizeof(*s));
	((handle*)s)->fd = fd;
	((handle*)s)->on_events = on_events;
	((handle*)s)->imp_engine_add = imp_engine_add;
	s->status = SOCKET_DATAGRAM;
	easy_close_on_exec(fd);
	return (handle*)s;
}

int32_t datagram_socket_recv(handle *h,iorequest *req,int32_t flag,int32_t *recvflags){
	socket_ *s = (socket_*)h;
	if(!h->e)
		return -ENOASSENG;
	else if(s->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	else if(!(s->status & SOCKET_DATAGRAM))
		return -EINVISOKTYPE;	
	errno = 0;
	if(flag == IO_NOW && list_size(&s->pending_recv)){
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
		int32_t bytes_transfer = TEMP_FAILURE_RETRY(recvmsg(((handle*)s)->fd,&_msghdr,0));					
		if(*recvflags) *recvflags = _msghdr.msg_flags;	
		if(bytes_transfer >= 0)
			return bytes_transfer;
		else if(errno != EAGAIN)
			return -errno;
	}
	list_pushback(&s->pending_recv,(listnode*)req);
	if(!is_read_enable(h)) enable_read(h);
	return -EAGAIN;	
}

int32_t datagram_socket_send(handle *h,iorequest *req){
	socket_ *s = (socket_*)h;
	if(!h->e)
		return -ENOASSENG;
	else if(s->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	else if(!(s->status & SOCKET_DATAGRAM))
		return -EINVISOKTYPE;	
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
	int32_t bytes_transfer = TEMP_FAILURE_RETRY(sendmsg(((handle*)s)->fd,&_msghdr,0));		
	if(bytes_transfer >= 0)
		return bytes_transfer;
	else
		return -errno;	
}