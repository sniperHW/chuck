#define _CORE_
#include "util/chk_signal.h"
#include "util/chk_error.h"
#include "event/chk_event_loop.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

extern int32_t pipe2(int pipefd[2], int flags);

#define MAX_SIGNAL_SIZE 64

typedef struct {
	_chk_handle;
    int32_t notify_fd;
    int32_t signo;
    signal_cb cb;
    void     *ud;
    void (*ud_dctor)(void*);
}chk_signal_handler;

static chk_signal_handler* signal_handlers[MAX_SIGNAL_SIZE] = {NULL};
static int32_t lock = 0;
#define LOCK() while (__sync_lock_test_and_set(&lock,1)) {}
#define UNLOCK() __sync_lock_release(&lock);


static void on_signal(int32_t signo) {
    chk_signal_handler *handler;
    LOCK();
    handler = signal_handlers[signo];
    if(handler){
        TEMP_FAILURE_RETRY(write(handler->notify_fd,&signo,sizeof(signo)));
    }
    UNLOCK();
}

static int32_t loop_add(chk_event_loop *e,chk_handle *h,chk_event_callback cb) {
	int32_t ret;
	chk_signal_handler *handler = cast(chk_signal_handler*,h);
	if(0 == (ret = chk_events_add(e,h,CHK_EVENT_READ)))
		handler->cb = cast(signal_cb,cb);
	return ret;
}

//must surround by LOCK() and UNLOCK()
static void release_signal_handler(chk_signal_handler *handler) {
	chk_events_remove(cast(chk_handle*,handler));
	close(handler->notify_fd);
	close(handler->fd);
	if(handler->ud_dctor) handler->ud_dctor(handler->ud);
	signal(handler->signo,SIG_DFL);
	signal_handlers[handler->signo] = NULL;
	free(handler);
}

static void on_events(chk_handle *h,int32_t events) {
	int32_t signo;
	size_t  size = sizeof(signo);
	chk_signal_handler *handler = cast(chk_signal_handler*,h);
	if(events == CHK_EVENT_LOOPCLOSE) {
		LOCK();
		release_signal_handler(handler);	
		UNLOCK();
		return;
	}else if(size == TEMP_FAILURE_RETRY(read(handler->fd,&signo,size)))
		handler->cb(handler->ud);	
}

static chk_signal_handler *signal_handler_new(int32_t signo,signal_cb cb,void *ud,void (*ud_dctor)(void*)) {
	int32_t fdpairs[2];
	chk_signal_handler *handler = calloc(1,sizeof(*handler));
    if(pipe2(fdpairs,O_NONBLOCK|O_CLOEXEC) != 0) {
    	free(handler);
        return NULL;	
    }
    handler->ud = ud;
    handler->notify_fd = fdpairs[1];
    handler->fd = fdpairs[0];
	handler->on_events = on_events;
	handler->handle_add = loop_add;
	handler->signo = signo;
	handler->ud_dctor = ud_dctor;
    return handler;
} 

int32_t chk_watch_signal(chk_event_loop *loop,int32_t signo,signal_cb cb,void *ud,void (*ud_dctor)(void*)) {
	int32_t ret;
	chk_signal_handler *handler;

	//以下信号禁止watch
	switch(signo){
		case SIGPIPE:return -1;
		case SIGSEGV:return -1;
		case SIGBUS:return -1;
		case SIGFPE:return -1;
		default:break;
	}

	LOCK();
	do{
		handler = signal_handlers[signo];
		if(handler) {
			ret = -1;
			break;
		}

		if(NULL == (handler = signal_handler_new(signo,cb,ud,ud_dctor))) {
			ret = -1;
			break;				
		}

		ret = chk_loop_add_handle(loop,cast(chk_handle*,handler),cb);
		if(0 != ret) {
			release_signal_handler(handler);
			break;
		}
		signal(signo,on_signal);
		signal_handlers[signo] = handler;	
	}while(0);
	UNLOCK();
	return ret;
}

void chk_unwatch_signal(int32_t signo) {
	chk_signal_handler *handler;
	LOCK();
	handler = signal_handlers[signo];
	if(handler) {
		release_signal_handler(handler);
	}
	UNLOCK();	
}

