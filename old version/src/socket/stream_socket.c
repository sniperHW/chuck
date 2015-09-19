#define __CORE__
#include <assert.h>
#include "socket/socket.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"

extern void    release_socket(socket_ *s);

typedef void(*stream_callback)(stream_socket_*,void*,int32_t,int32_t);

static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback)
{
	int32_t ret;
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	ret = event_add(e,h,EVENT_READ) || event_add(e,h,EVENT_WRITE);
	if(ret == 0){
		easy_noblock(h->fd,1);
		cast(stream_socket_*,h)->callback = cast(stream_callback,callback);
	}
	return ret;
}

 
static void process_read(stream_socket_ *s,int32_t events)
{
	iorequest *req = NULL;
	int32_t bytes = 0;
	req = cast(iorequest*,list_pop(&s->pending_recv);

	if(!req && (events & (EPOLLERR | EPOLLHUP |EPOLLRDHUP)))
	{
		s->callback(s,req,0,0);
		if(!(s->status & SOCKET_CLOSE)){
			engine_remove(cast(handle*,s));
			close(cast(handle*,s)->fd);
		}
	}else {
		s->status |= SOCKET_READABLE;
		errno = 0;
		bytes = TEMP_FAILURE_RETRY(readv(cast(handle*,s)->fd,req->iovec,req->iovec_count));	
		s->callback(s,req,bytes,errno);
		if(s->status & SOCKET_CLOSE)
			return;			
		if(s->e && !list_size(&s->pending_recv)){
			//没有接收请求了,取消EPOLLIN
			disable_read(cast(handle*,s));
		}
	}	
}

static void process_write(stream_socket_ *s)
{
	iorequest *req = 0;
	int32_t bytes = 0;
	disable_write(cast(handle*,s));
	s->status |= SOCKET_WRITEABLE;
	if((req = cast(iorequest*,list_pop(&s->pending_send)))!=NULL){
		errno = 0;	
		bytes = TEMP_FAILURE_RETRY(writev(s->fd,req->iovec,req->iovec_count));
		s->callback(s,req,bytes,errno);
		if(s->status & SOCKET_CLOSE)
			return;				
	}	
}

static void on_events(handle *h,int32_t events)
{
	stream_socket_ *s = cast(stream_socket_*,h);
	if(!s->e || s->status & SOCKET_CLOSE)
		return;
	if(events == EVENT_ECLOSE){
		s->callback(s,NULL,-1,EENGCLOSE);
		return;
	}
	do{
		s->status |= SOCKET_INLOOP;
		if(events & EVENT_READ){
			process_read(s,events);	
			if(s->status & SOCKET_CLOSE) 
				break;								
		}		
		if(s->e && (events & EVENT_WRITE))
			process_write(s);			
		s->status ^= SOCKET_INLOOP;
	}while(0);
	if(s->status & SOCKET_CLOSE){
		release_socket((socket_*)s);		
	}
}


void stream_socket_init(stream_socket_ *s,int32_t fd)
{
	s->on_events = on_events;
	s->imp_engine_add = imp_engine_add;
	s->type = STREAM;
	s->fd = fd;
	s->on_events = on_events;
	s->imp_engine_add = imp_engine_add;
	s->status = SOCKET_READABLE | SOCKET_WRITEABLE;
	easy_close_on_exec(fd);		
}	

stream_socket_ *new_stream_socket(int32_t fd)
{
	stream_socket_ *s = calloc(1,sizeof(*s));
	stream_socket_init(s,fd);
	return s;
}

int32_t stream_socket_recv(stream_socket_ *s,iorequest *req,int32_t flag)
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
		bytes = TEMP_FAILURE_RETRY(readv(h->fd,req->iovec,req->iovec_count));
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

int32_t stream_socket_send(stream_socket_ *s,iorequest *req,int32_t flag)
{
	int32_t bytes;
	handle *h = cast(handle*,s);
	if(s->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	else if(!h->e)
		return -ENOASSENG;

	errno = 0;
	if(s->status & SOCKET_WRITEABLE && 
	   flag == IO_NOW && 
	   !list_size(&s->pending_send))
	{
		bytes = TEMP_FAILURE_RETRY(writev(h->fd,req->iovec,req->iovec_count));
		if(bytes >= 0)
			return bytes;
		else if(errno != EAGAIN)
			return -errno;
	}
	s->status ^= SOCKET_WRITEABLE;
	list_pushback(&s->pending_send,cast(listnode*,req));
	if(!is_write_enable(h)) enable_write(h);
	return -EAGAIN;	
}
