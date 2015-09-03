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

static void _log_stack(int32_t logLev,int32_t start,const char *prefix,void **bt);

void chk_exp_log_exption_stack() {
	char    buff[256];
	chk_expn_thd *expthd = chk_exp_get_thread_expn();
	if(!expthd->sz) return;
    if(expthd->exception == segfault)
    	snprintf(buff,256," [exception:%s <invaild access addr:%p>]",
                 segfault,expthd->addr);
    else
    	snprintf(buff,256," [exception:%s]",expthd->exception);	
	_log_stack(LOG_ERROR,3,buff,expthd->bt);
}

static void exception_throw(chk_expn_frame *frame,const char *exception,siginfo_t* info) {
	int32_t sig = 0;
	chk_expn_thd *expthd = chk_exp_get_thread_expn();
	if(!frame) {
		expthd->sz = backtrace(expthd->bt, LOG_STACK_SIZE);
		expthd->exception = exception;
		expthd->addr = info->si_addr;
		frame = chk_exp_top();
	}
	if(frame) {
		frame->is_process = 0;
		if(exception == segfault) sig = SIGSEGV;
		else if(exception == sigbug) sig = SIGBUS;
		else if(exception == sigfpe) sig = SIGFPE;  
		siglongjmp(frame->jumpbuffer,sig);
	}else {
		chk_exp_log_exption_stack();
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
	exception_throw(frame,chk_exp_get_thread_expn()->exception,NULL);
}

void chk_exp_log_call_stack(const char *discription) {
	_log_stack(LOG_DEBUG,2,discription,NULL);
}

static inline void *getaddr(char *in) {
	int32_t  b,i,size = 32;
	char     buf[32];
	char     *out = buf;
	void     *addr = NULL;
	for(i = b = 0;i < size - 1;in++)
		switch(*in) {
			case '[':{if(!b){b = 1; break;} else return NULL;}
			case ']':{
				if(b){
					*out++ = 0;
					sscanf(buf,"%LX",(long long unsigned int *)&addr);
					return addr;
				}
				return NULL;
			}
			default:{if(b){*out++ = *in;++i;}break;}
		}
	return NULL;
}

static void *getsoaddr(char *path,void *addr) {
	FILE   *pipe;
	void   *soaddr = NULL;
	char    buff[1024]={0};
	int32_t i =0;
	char *ptr = path + strlen(path);
	for(;ptr != path;--ptr) if((*ptr) == '/'){ ++ptr;break;}
	snprintf(buff,1024,"pmap -x %d",getpid());
	pipe = popen(buff, "r");
	if(!pipe) return NULL;
	for(; i < 1024; ++i){
		if(1 != fread(&buff[i],1,1,pipe)){
			pclose(pipe);
			return NULL;
		}else if(buff[i] == '\n'){
			buff[i] = 0;
			if(strstr(buff,ptr)){
				for(i=0;buff[i] != ' ';++i);
				buff[i] = 0;
				sscanf(&buff[0],"%LX",(long long unsigned int *)&soaddr);
				*(uint64_t*)&soaddr = addr - soaddr;
				break;	
			}
			i = -1;
		}
	}
	pclose(pipe);
	return soaddr;
}

static int32_t getdetail(char *str,char *output,int32_t size) {
	void *addr;
	char  path[256]={0};
	char  cmd[1024]={0};
	FILE *pipe;
	int   i,j;
	char *so = strstr(str,".so");
	if(so){
		strncpy(path,str,(so-str)+3);
		if(!(addr = getaddr(str))) return -1;
		if(!(addr = getsoaddr(path,addr))) return -1;
	}
	else{
		if(0 >= readlink("/proc/self/exe", path, 256))
			return -1;
		else if(!(addr = getaddr(str))) return -1;
	}

	snprintf(cmd,1024,"addr2line -fCse %s %Lx", path, (long long unsigned int)addr);
	pipe = popen(cmd, "r");
	if(!pipe) return -1;
	i = fread(output,1,size-1,pipe);	
	pclose(pipe);
	output[i] = '\0';
	for(j=0; j<=i; ++j) if(output[j] == '\n') output[j] = ' ';	
	return 0;
}

static void _log_stack(int32_t logLev,int32_t start,const char *prefix,void **bt) {
	char**                  strings;
	size_t                  sz;
	int32_t                 i,f;
	int32_t 				size = 0;	
	char 					logbuf[CHK_MAX_LOG_SIZE];
	char                    buf[1024];
	char 				   *ptr;
	void                   *_bt[LOG_STACK_SIZE];

	if(bt){
		sz      = chk_exp_get_thread_expn()->sz;
		strings = backtrace_symbols(bt, sz);
	}else{
		sz      = backtrace(_bt, LOG_STACK_SIZE);
		strings = backtrace_symbols(_bt, sz);
	}
	if(prefix) size += snprintf(logbuf,CHK_MAX_LOG_SIZE,"%s\n",prefix);	    		    			
	for(i = start,f = 0; i < sz; ++i) {
		if(strstr(strings[i],"main+")) break;
		ptr  = logbuf + size;
		if(0 == getdetail(strings[i],buf,1024))
			size += snprintf(ptr,CHK_MAX_LOG_SIZE-size,
				"\t% 2d: %s %s\n",++f,strings[i],buf);
		else
			size += snprintf(ptr,CHK_MAX_LOG_SIZE-size,
				"\t% 2d: %s\n",++f,strings[i]);		
	}
	CHK_SYSLOG(logLev,"%s",logbuf);
	free(strings);	
}