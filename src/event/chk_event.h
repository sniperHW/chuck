#ifndef _CHK_EVENT_H
#define _CHK_EVENT_H

#include    <unistd.h>
#include	<sys/types.h>	/* basic system data types */
#include	<sys/socket.h>	/* basic socket definitions */
#include	<sys/time.h>	/* timeval{} for select() */
#include	<time.h>		/* timespec{} for pselect() */
#include	<netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include	<arpa/inet.h>	/* inet(3) functions */
#include	<errno.h>
#include	<fcntl.h>		/* for nonblocking */
#include	<netdb.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>	/* for S_xxx file mode constants */
#include	<sys/uio.h>		/* for iovec{} and readv/writev */
#include	<unistd.h>
#include	<sys/wait.h>
#include	<sys/un.h>		/* for Unix domain sockets */
#include    <net/if.h>
#include    <sys/ioctl.h>
#include    <netinet/tcp.h>
#include    <fcntl.h>
#include    <stdint.h>


#include    "util/chk_list.h"



typedef struct chk_event_loop chk_event_loop;

typedef struct chk_handle   chk_handle;

typedef void* chk_event_callback;


#ifndef _chk_handle
#define _chk_handle                                                         \
    chk_dlist_entry entry;                                                  \
    chk_dlist_entry ready_entry;                                            \
    int32_t         fd;                                                     \
    int32_t         active_evetns;/*激活的事件*/                            \
    int32_t         events;         /*关注的事件*/                          \
    chk_event_loop *loop;                                                   \
    int32_t (*handle_add)(chk_event_loop*,chk_handle*,chk_event_callback);  \
    void    (*on_events)(chk_handle*,int32_t events)                    

#endif

struct chk_handle {
    _chk_handle;
};


#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)                                      \
    ({ long int __result;                                                   \
    do __result = (long int)(expression);                                   \
    while(__result == -1L&& errno == EINTR);                                \
    __result;})
#endif 


#ifdef _CORE_

#ifdef _LINUX

#include    <sys/epoll.h>
extern int32_t pipe2(int pipefd[2], int flags);

enum{
    CHK_EVENT_READ   =  EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP,
    CHK_EVENT_WRITE  =  EPOLLOUT,
    CHK_EVENT_LOOPCLOSE = 0x7fffffff,//engine close    
};

#elif _MACH

#   include    <sys/event.h>

enum{
    CHK_EVENT_READ   =  1,
    CHK_EVENT_WRITE  =  1 << 2,
    CHK_EVENT_LOOPCLOSE = 0x7fffffff,//engine close         
};

#else

#   error "un support platform!"   

#endif

int32_t chk_watch_handle(chk_event_loop*,chk_handle*,int32_t events);

int32_t chk_unwatch_handle(chk_handle*);

int32_t chk_events_enable(chk_handle*,int32_t events);

int32_t chk_events_disable(chk_handle*,int32_t events);

int32_t chk_is_read_enable(chk_handle*h);

int32_t chk_is_write_enable(chk_handle*h);

static inline int32_t chk_enable_read(chk_handle *h) {
    return chk_events_enable(h,CHK_EVENT_READ);
}

static inline int32_t chk_disable_read(chk_handle *h) {
    return chk_events_disable(h,CHK_EVENT_READ);
}

static inline int32_t chk_enable_write(chk_handle *h) {   
    return chk_events_enable(h,CHK_EVENT_WRITE);
}

static inline int32_t chk_disable_write(chk_handle *h) {
    return chk_events_disable(h,CHK_EVENT_WRITE);         
}


#endif

#endif
