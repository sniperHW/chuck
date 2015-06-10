#include <assert.h>
#include <sys/timerfd.h>
#include "util/timerfd.h"
#include "engine/engine.h"


static int32_t 
imp_engine_add(engine *e,handle *h,
			   generic_callback callback)
{
	int32_t ret;
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	ret = event_add(e,h,EVENT_READ);
	if(ret == 0){
		cast(timerfd*,h)->callback = cast(void (*)(void*),callback);
	}
	return ret;
}

static void 
on_timeout(handle *h,int32_t events)
{
	int64_t _;
	if(events == EENGCLOSE)
		return;
	TEMP_FAILURE_RETRY(read(h->fd,&_,sizeof(_)));	
	cast(timerfd*,h)->callback(cast(timerfd*,h)->ud);
}

timerfd*
timerfd_new(uint32_t timeout,void *ud)
{
	struct itimerspec spec;
    struct timespec now;
	int32_t sec,ms;  
	int64_t nosec;    
	timerfd *t = calloc(1,sizeof(*t));

	cast(handle*,t)->fd = timerfd_create(CLOCK_MONOTONIC,TFD_CLOEXEC|TFD_NONBLOCK);
	if(cast(handle*,t)->fd < 0){
		free(t);
		return NULL;
	}
	clock_gettime(CLOCK_MONOTONIC, &now);
	sec = timeout/1000;
	ms = timeout%1000;    
	nosec = (now.tv_sec + sec)*1000*1000*1000 + now.tv_nsec + ms*1000*1000;
	spec.it_value.tv_sec = nosec/(1000*1000*1000);
    	spec.it_value.tv_nsec = nosec%(1000*1000*1000);
    	spec.it_interval.tv_sec = sec;
    	spec.it_interval.tv_nsec = ms*1000*1000;	
	
	if(0 != timerfd_settime(cast(handle*,t)->fd,TFD_TIMER_ABSTIME,&spec,0))
	{
		close(cast(handle*,t)->fd);
		free(t);
		return NULL;
	}
	cast(handle*,t)->on_events = on_timeout;
	cast(handle*,t)->imp_engine_add = imp_engine_add;
	t->ud = ud;
	return t;
}

void 
timerfd_del(timerfd *t)
{
	close(cast(handle*,t)->fd);
	free(t);
}