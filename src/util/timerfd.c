#include <assert.h>
#include <sys/timerfd.h>
#include "util/timerfd.h"
#include "engine/engine.h"


static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback)
{
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	int32_t ret = event_add(e,h,EVENT_READ);
	if(ret == 0){
		((timerfd*)h)->callback = (void (*)(void*))callback;
	}
	return ret;
}

static void on_timeout(handle *h,int32_t events){
	int64_t _;
	TEMP_FAILURE_RETRY(read(h->fd,&_,sizeof(_)));	
	((timerfd*)h)->callback(((timerfd*)h)->ud);
}

handle *timerfd_new(uint32_t timeout,void *ud){
	timerfd *t = calloc(1,sizeof(*t));

	((handle*)t)->fd = timerfd_create(CLOCK_MONOTONIC,TFD_CLOEXEC|TFD_NONBLOCK);
	if(((handle*)t)->fd < 0){
		free(t);
		return NULL;
	}
	struct itimerspec spec;
    struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	int32_t sec = timeout/1000;
	int32_t ms = timeout%1000;    
	int64_t nosec = (now.tv_sec + sec)*1000*1000*1000 + now.tv_nsec + ms*1000*1000;
	spec.it_value.tv_sec = nosec/(1000*1000*1000);
    	spec.it_value.tv_nsec = nosec%(1000*1000*1000);
    	spec.it_interval.tv_sec = sec;
    	spec.it_interval.tv_nsec = ms*1000*1000;	
	
	if(0 != timerfd_settime(((handle*)t)->fd,TFD_TIMER_ABSTIME,&spec,0))
	{
		close(((handle*)t)->fd);
		free(t);
		return NULL;
	}
	((handle*)t)->on_events = on_timeout;
	((handle*)t)->imp_engine_add = imp_engine_add;
	t->ud = ud;
	return (handle*)t;
}

void timerfd_del(handle *h){
	close(h->fd);
	free(h);
}