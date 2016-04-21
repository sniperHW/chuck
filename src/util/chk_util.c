#define _CORE_
#include "util/chk_util.h"
#include "util/chk_error.h"
#include "event/chk_event.h"

extern int32_t easy_noblock(int32_t fd,int32_t noblock);

int32_t chk_create_notify_channel(int32_t fdpairs[2]) {
#ifdef _LINUX
    if(pipe2(fdpairs,O_NONBLOCK|O_CLOEXEC) != 0) {
        return chk_error_create_notify_channel;	
    }
#elif _MACH
	if(pipe(fdpairs) != 0){
		return chk_error_create_notify_channel;
	}
	easy_noblock(fdpairs[0],1);		
	easy_noblock(fdpairs[1],1);
	fcntl(fdpairs[0],F_SETFD,FD_CLOEXEC);
	fcntl(fdpairs[1],F_SETFD,FD_CLOEXEC);
#else
#   error "un support platform!" 
#endif
	return chk_error_ok;
}

void chk_close_notify_channel(int32_t fdpairs[2]) {
	close(fdpairs[0]);
	close(fdpairs[1]);	
} 
