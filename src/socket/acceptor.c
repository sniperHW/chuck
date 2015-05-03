#include <assert.h>
#include "socket/acceptor.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"

static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback)
{
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	int32_t ret = event_add(e,h,EVENT_READ);
	if(ret == 0){
		easy_noblock(h->fd,1);
		((acceptor*)h)->callback = (void (*)(int32_t fd,sockaddr_*,void*))callback;
	}
	return ret;
}


static int _accept(handle *h,sockaddr_ *addr){
	socklen_t len = 0;
	int32_t fd; 
	while((fd = accept(h->fd,(struct sockaddr*)addr,&len)) < 0){
#ifdef EPROTO
		if(errno == EPROTO || errno == ECONNABORTED)
#else
		if(errno == ECONNABORTED)
#endif
			continue;
		else
			return -errno;
	}
	return fd;
}

static void process_accept(handle *h,int32_t events){
    int fd;
    sockaddr_ addr;
    for(;;){
    	fd = _accept(h,&addr);
    	if(fd < 0)
    	   break;
    	else{
		   ((acceptor*)h)->callback(fd,&addr,((acceptor*)h)->ud);
    	}      
    }
}

handle *acceptor_new(int32_t fd,void *ud){
	acceptor *a = calloc(1,sizeof(*a));
	a->ud = ud;
	((handle*)a)->fd = fd;
	((handle*)a)->on_events = process_accept;
	((handle*)a)->imp_engine_add = imp_engine_add;
	easy_close_on_exec(fd);
	return (handle*)a;
}

void    acceptor_del(handle *h){
	close(h->fd);
	free(h);
} 