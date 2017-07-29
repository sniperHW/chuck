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

static __thread chk_expn_thd *_exception_st = NULL;

static void _log_stack(int32_t logLev,int32_t start,const char *prefix,void **bt);

void chk_exp_log_exption_stack() {
	char    buff[256] = {0};
	chk_expn_thd *expthd = chk_exp_get_thread_expn();
	if(!expthd->sz) return;
    if(expthd->exception == segfault)
    	snprintf(buff,sizeof(buff) - 1," [exception:%s <invaild access addr:%p>]",
                 segfault,expthd->addr);
    else
    	snprintf(buff,sizeof(buff) - 1," [exception:%s]",expthd->exception);	
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
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_sigaction = signal_segv;
	action.sa_flags = SA_SIGINFO;
	if(sigaction(SIGSEGV, &action, NULL) < 0) {
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

__attribute__((constructor(103))) static void chk_sig_init() {
	setup_sigsegv();
}

chk_expn_thd *chk_exp_get_thread_expn() {
    if(!_exception_st) {
        _exception_st = calloc(1,sizeof(*_exception_st));
		chk_list_init(&_exception_st->expstack);	
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

#ifndef _MACH

static void *getaddr(char *in) {
/*
#ifdef _MACH
	int32_t  wc,w,i,size = 32;
	char     buf[32];
	char     *out = buf;
	void     *addr = NULL;
	for(wc = w = i = 0; *in !=0 && i < size - 1;in++)
		switch(*in) {
			case ' ':{
				if(1 == w) {
					//end of a word
					w = 0;
					if(wc == 3) {
						*out++ = 0;
						sscanf(buf,"%p",&addr);
						return addr;
					}
				}								
			}break;
			default:{
				if(w == 0){
					//begin a new word
					w = 1;
					++wc;
				}
				if(3 == wc){
					//only copy word 3
					*out++ = *in;
					++i;
				}
			}break;
		}	
#else
*/
	int32_t  b,i,size = 32;
	char     buf[32];
	char     *out = buf;
	void     *addr = NULL;
	for(i = b = 0; *in !=0 && i < size - 1;in++)
		switch(*in) {
			case '[':{if(!b){b = 1; break;} else return NULL;}
			case ']':{
				if(b){
					*out++ = 0;
					sscanf(buf,"%p",&addr);
					return addr;
				}
				return NULL;
			}
			default:{if(b){*out++ = *in;++i;}break;}
		}
//#endif
	return NULL;
}

static void *getsoaddr(const char *str,char *path) {

	//1: ./lib/chuck.so(+0xb3a73) [0x7f68a2e76a73]
	void *addr;
	char buf[32];
	size_t i = 0;
	char *s = strstr(str,".so");
	if(!s) {
		return NULL;
	}
	size_t size = s - str + 3;
	for(i = 0; i < size; ++i) {
		path[i] = str[i];
	}
	path[size] = 0;

	s = strstr(s,"0x");
	if(!s) {
		return NULL;
	}
	for(i = 0 ; i < 31; ++i) {
		buf[i] = s[i];
		if(buf[i] == ')') {
			buf[i] = 0;
			sscanf(buf,"%p",&addr);
			return addr;
		}else if(buf[i] == 0) {
			return NULL;
		}
	}
	return NULL;

/*	FILE   *pipe;
	void   *soaddr = NULL;
	char    buff[1024]={0};
	int32_t i =0;
	char *ptr = path + strlen(path);
	for(;ptr != path;--ptr) if((*ptr) == '/'){ ++ptr;break;}
	snprintf(buff,sizeof(buff) - 1,"pmap -x %d",getpid());
	pipe = popen(buff, "r");
	if(!pipe) return NULL;
	for(; i < sizeof(buff); ++i){
		if(1 != fread(&buff[i],1,1,pipe)){
			pclose(pipe);
			return NULL;
		}else if(buff[i] == '\n'){
			buff[i] = 0;
			if(strstr(buff,ptr)){
				for(i=0;buff[i] != ' ';++i);
				buff[i] = 0;
				sscanf(&buff[0],"%p",&soaddr);
				*(uint64_t*)&soaddr = addr - soaddr;
				break;	
			}
			i = -1;
		}
	}
	pclose(pipe);
	return soaddr;
*/

}

#endif

static int32_t getdetail(char *str,char *output,int32_t size) {

#ifdef _MACH
	return -1;
#else

	void *addr;
	char  path[1024]={0};
	char  cmd[1024]={0};
	FILE *pipe;
	int   i,j;
	char *so = strstr(str,".so");
	if(so) {
		if(!(addr = getsoaddr(str,path))) return -1;
	} else{
		if(0 >= readlink("/proc/self/exe", path, 1024))
			return -1;
		else if(!(addr = getaddr(str))) return -1;
	}

	snprintf(cmd,sizeof(cmd) - 1,"addr2line -fCse %s %p", path,addr);
	pipe = popen(cmd, "r");
	if(!pipe) return -1;
	i = fread(output,1,size-1,pipe);	
	pclose(pipe);
	output[i] = '\0';
	for(j=0; j<=i; ++j) if(output[j] == '\n') output[j] = ' ';	
	return 0;
#endif
}

static void _log_stack(int32_t logLev,int32_t start,const char *prefix,void **bt) {
	char**                  strings;
	size_t                  sz;
	int32_t                 i,f;
	int32_t 				size;	
	char                   *logbuf;
	char                    buf[1024] = {0};
	char 				   *ptr;
	void                   *_bt[LOG_STACK_SIZE] = {0};

    if(chk_current_loglev() > logLev) return;    
    logbuf = malloc(CHK_MAX_LOG_SIZE);
    if(!logbuf) return;
    size = chk_log_prefix(logbuf,logLev);
	if(bt){
		sz      = chk_exp_get_thread_expn()->sz;
		strings = backtrace_symbols(bt, sz);
	}else{
		sz      = backtrace(_bt, LOG_STACK_SIZE);
		strings = backtrace_symbols(_bt, sz);
	}
	if(prefix) size += snprintf(&logbuf[size],CHK_MAX_LOG_SIZE-size-1,"%s\n",prefix);	    		    			
	for(i = start,f = 0; i < sz; ++i) {
		if(strstr(strings[i],"main+")) break;
		ptr  = logbuf + size;
		if(0 == getdetail(strings[i],buf,1024))
			size += snprintf(ptr,CHK_MAX_LOG_SIZE-size-1,
				"\t% 2d: %s %s\n",++f,strings[i],buf);
		else
			size += snprintf(ptr,CHK_MAX_LOG_SIZE-size-1,
				"\t% 2d: %s\n",++f,strings[i]);		
	}
	chk_syslog(logLev,logbuf);
	free(strings);	
}
