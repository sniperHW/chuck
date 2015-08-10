#include <stdlib.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include "util/chk_exception.h"
#include "util/chk_log.h"

static const char *segfault   = "segfault";
static const char *sigbug     = "sigbug";
static const char *sigfpe     = "sigfpe";

static __thread chk_expn_thd *_exception_st = NULL;

static void _log_stack(int32_t logLev,int32_t start,char *str);

static void exception_throw(chk_expn_frame *frame,const char *exception,siginfo_t* info) {
	int32_t sig = 0;
	char    buff[256];
	if(!frame) {
		//非rethrow,打印日志
        if(exception == segfault)
    	    snprintf(buff,256," [exception:%s <invaild access addr:%p>]\n",
                             segfault,info->si_addr);
        else
    	    snprintf(buff,256," [exception:%s]\n",exception);		
		_log_stack(LOG_ERROR,3,buff);
		frame = chk_exp_top();
	}
	if(frame) {
		frame->exception = exception;
		frame->is_process = 0;
		if(exception == segfault) sig = SIGSEGV;
		else if(exception == sigbug) sig = SIGBUS;
		else if(exception == sigfpe) sig = SIGFPE;  
		siglongjmp(frame->jumpbuffer,sig);
	}else {
		//没有try,直接终止进程
		exit(0);
	}		
}


static void signal_segv(int32_t signum,siginfo_t* info,void*ptr) {
	exception_throw(NULL,segfault,info);
}

int32_t setup_sigsegv() {
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = signal_segv;
	action.sa_flags = SA_SIGINFO;
	if(sigaction(SIGSEGV, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}
	return 1;
}

static void signal_sigbus(int32_t signum,siginfo_t* info,void*ptr) {
	exception_throw(NULL,sigbug,info);
}

int32_t setup_sigbus() {
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = signal_sigbus;
	action.sa_flags = SA_SIGINFO;
	if(sigaction(SIGBUS, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}
	return 1;
}

static void signal_sigfpe(int signum,siginfo_t* info,void*ptr) {
	exception_throw(NULL,sigfpe,info);
}

int32_t setup_sigfpe() {
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = signal_sigfpe;
	action.sa_flags = SA_SIGINFO;
	if(sigaction(SIGFPE, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}
	return 1;
}

static void reset_perthread_exception_st()
{
	if(_exception_st) {
		free(_exception_st);
		_exception_st = NULL;
	}
}


chk_expn_thd *chk_exp_get_thread_expn() {
    if(!_exception_st) {
        _exception_st = calloc(1,sizeof(*_exception_st));
		chk_list_init(&_exception_st->expstack);	
		setup_sigsegv();
		setup_sigbus();
		setup_sigfpe();
		pthread_atfork(NULL,NULL,reset_perthread_exception_st);        
    }
    return _exception_st;
}

void chk_exp_throw(const char *exp) {
	exception_throw(NULL,exp,NULL);
}

void chk_exp_rethrow(chk_expn_frame *frame) {
	exception_throw(frame,frame->exception,NULL);
}

void chk_exp_log_stack() {
	_log_stack(LOG_DEBUG,2," [call_stack]\n");
}


static int32_t addr2line(char *addr,char *output,int32_t size) {		
	char path[256]={0};
	char cmd[1024]={0};
	FILE *pipe;
	int  i,j;
	if(0 >= readlink("/proc/self/exe", path, 256))
		return -1;
	snprintf(cmd,1024,"addr2line -fCse %s %s", path, addr);
	pipe = popen(cmd, "r");
	if(!pipe) return -1;
	i = fread(output,1,size-1,pipe);	
	pclose(pipe);
	output[i] = '\0';
	for(j=0; j<=i; ++j) if(output[j] == '\n') output[j] = ' ';	
	return 0;
}

static inline int32_t getaddr(char *in,char *out,size_t size) {
	int32_t b,i;
	for(i = b = 0;i < size - 1;in++) switch(*in) {
		case '[':{if(!b){b = 1; break;} else return 0;}
		case ']':{if(b){*out++ = 0;return 1;}return 0;}
		default:{if(b){*out++ = *in;++i;}break;}
	}
	return 0;
}

static void _log_stack(int32_t logLev,int32_t start,char *str) {
	void     *bt[64];
	char    **strings;
	char 	 *ptr;	
	size_t    sz;
	int32_t   i,f;
	int32_t   size = 0;	
	char 	  logbuf[CHK_MAX_LOG_SIZE];
	char      addr[32],buf[1024];		
	sz      = backtrace(bt, 64);
	strings = backtrace_symbols(bt, sz);
	size   += snprintf(logbuf,CHK_MAX_LOG_SIZE,"%s",str);	    		    			
	for(i = start,f = 0; i < sz; ++i) {
		if(strstr(strings[i],"main+")) break;
		ptr  = logbuf + size;
		if(getaddr(strings[i],addr,32) && !addr2line(addr,buf,1024))
			size += snprintf(ptr,CHK_MAX_LOG_SIZE-size,
				"\t% 2d: %s %s\n",++f,strings[i],buf);
		else
			size += snprintf(ptr,CHK_MAX_LOG_SIZE-size,
				"\t% 2d: %s\n",++f,strings[i]);		
	}
	CHK_SYSLOG(logLev,"%s",logbuf);
	free(strings);	
}