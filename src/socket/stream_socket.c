#include <assert.h>
#include "socket/socket.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"

extern int32_t is_read_enable(handle*h);
extern int32_t is_write_enable(handle*h);
extern void    release_socket(socket_ *s);

typedef void(*stream_callback)(stream_socket_*,void*,int32_t,int32_t);

static int32_t 
imp_engine_add(engine *e,handle *h,
			   generic_callback callback)
{
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	int32_t ret;
#ifdef _LINUX
	ret = event_add(e,h,EPOLLRDHUP);
#elif   _BSD
	if((ret = event_add(e,h,EVFILT_READ)) == 0 &&
	   (ret = event_add(e,h,EVFILT_READ)) == 0){
		disable_read(h);
		disable_write(h)
	}else if(h->e){
		event_remove(h);
	}
#else
	return -EUNSPPLAT;
#endif
	if(ret == 0){
		easy_noblock(h->fd,1);
		((stream_socket_*)h)->callback = (stream_callback)callback;
	}
	return ret;
}

#define pending_recv(S) (((socket_*)S)->pending_recv)
#define status(S)       (((socket_*)S)->status)
#define type(S)         (((socket_*)S)->type)  

static void 
process_read(stream_socket_ *s)
{
	iorequest *req = NULL;
	int32_t bytes = 0;
	while((req = (iorequest*)list_pop(&pending_recv(s)))!=NULL){
		errno = 0;
		bytes = TEMP_FAILURE_RETRY(readv(((handle*)s)->fd,req->iovec,req->iovec_count));	
		
		if(bytes < 0 && errno == EAGAIN){
				//将请求重新放回到队列
				list_pushback(&pending_recv(s),(listnode*)req);
				break;
		}else{
			s->callback(s,req,bytes,errno);
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
process_write(stream_socket_ *s)
{
	iorequest *req = 0;
	int32_t bytes = 0;
	while((req = (iorequest*)list_pop(&s->pending_send))!=NULL){
		errno = 0;	
		bytes = TEMP_FAILURE_RETRY(writev(((handle*)s)->fd,req->iovec,req->iovec_count));
		if(bytes < 0 && errno == EAGAIN){
				//将请求重新放回到队列
				list_pushback(&s->pending_send,(listnode*)req);
				break;
		}else{
			s->callback(s,req,bytes,errno);
			if(status(s) & SOCKET_CLOSE)
				return;
		}
	}
	if(!list_size(&s->pending_send)){
		//没有接收请求了,取消EPOLLOUT
		disable_write((handle*)s);
	}		
}

static void 
on_events(handle *h,int32_t events)
{
	stream_socket_ *s = (stream_socket_*)h;
	if(status(s) & SOCKET_CLOSE)
		return;
	do{
		status(s) |= SOCKET_INLOOP;
		if(events & EVENT_READ){
			process_read(s);	
			if(status(s) & SOCKET_CLOSE) 
				break;								
		}		
		if(events & EVENT_WRITE)
			process_write(s);			
		status(s) ^= SOCKET_INLOOP;
	}while(0);
	if(status(s) & SOCKET_RELEASE){
		release_socket((socket_*)s);		
	}
}


void    
construct_stream_socket(stream_socket_ *s)
{
	((handle*)s)->on_events = on_events;
	((handle*)s)->imp_engine_add = imp_engine_add;
	type(s) = STREAM;	
}	

stream_socket_*
new_stream_socket(int32_t fd)
{
	stream_socket_ *s = calloc(1,sizeof(*s));
	((handle*)s)->fd = fd;
	((handle*)s)->on_events = on_events;
	((handle*)s)->imp_engine_add = imp_engine_add;
	type(s) = STREAM;
	easy_close_on_exec(fd);
	return s;
}

int32_t 
stream_socket_recv(stream_socket_ *s,
				   iorequest *req,int32_t flag)
{
	handle *h = (handle*)s;
	if(!h->e)
		return -ENOASSENG;
	else if(status(s) & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	errno = 0;
	if(flag == IO_NOW && list_size(&pending_recv(s))){
		int32_t bytes = TEMP_FAILURE_RETRY(readv(h->fd,req->iovec,req->iovec_count));
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
stream_socket_send(stream_socket_ *s,
				   iorequest *req,int32_t flag)
{
	handle *h = (handle*)s;
	if(!h->e)
		return -ENOASSENG;
	else if(status(s) & SOCKET_CLOSE)
		return -ESOCKCLOSE;

	errno = 0;
	if(flag == IO_NOW && list_size(&s->pending_send)){
		int32_t bytes = TEMP_FAILURE_RETRY(writev(h->fd,req->iovec,req->iovec_count));
		if(bytes >= 0)
			return bytes;
		else if(errno != EAGAIN)
			return -errno;
	}
	list_pushback(&s->pending_send,(listnode*)req);
	if(!is_write_enable(h)) enable_write(h);
	return -EAGAIN;	
}