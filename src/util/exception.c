#include <stdlib.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include "exception.h"
#include "log.h"

int32_t 
setup_sigsegv();

static __thread exception_perthd_st *__perthread_exception_st = NULL;

static void signal_segv(int32_t signum,siginfo_t* info,void*ptr)
{
	exception_throw(except_segv_fault,__FILE__,__FUNCTION__,__LINE__,info);
}

int32_t setup_sigsegv()
{
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

static void signal_sigbus(int32_t signum,siginfo_t* info,void*ptr)
{
	THROW(except_sigbus);
}

int32_t setup_sigbus()
{
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

static void signal_sigfpe(int signum,siginfo_t* info,void*ptr)
{
	THROW(except_arith);
}

int32_t setup_sigfpe()
{
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
	if(__perthread_exception_st){
		while(list_size(&__perthread_exception_st->csf_pool))
			free(list_pop(&__perthread_exception_st->csf_pool));
		free(__perthread_exception_st);
		__perthread_exception_st = NULL;
	}
}


exception_perthd_st *__get_perthread_exception_st()
{
	int32_t i;
    if(!__perthread_exception_st){
        __perthread_exception_st = calloc(1,sizeof(*__perthread_exception_st));
		list_init(&__perthread_exception_st->expstack);	
		list_init(&__perthread_exception_st->csf_pool);
		for(i = 0;i < 256; ++i)
			list_pushfront(&__perthread_exception_st->csf_pool,
						   (listnode*)calloc(1,sizeof(callstack_frame)));
		setup_sigsegv();
		setup_sigbus();
		setup_sigfpe();
		pthread_atfork(NULL,NULL,reset_perthread_exception_st);        
    }
    return __perthread_exception_st;
}


static inline callstack_frame *get_csf(list *pool)
{
	int32_t i;
	callstack_frame *call_frame;
	if(!list_size(pool))
	{
		for(i = 0;i < 256; ++i){
			call_frame = calloc(1,sizeof(*call_frame));
			list_pushfront(pool,&call_frame->node);
		}
	}
	return  cast(callstack_frame*,list_pop(pool));
}

static int32_t addr2line(const char *addr,char *output,int32_t size)
{		
	char path[256]={0};
	char cmd[1024]={0};
	FILE *pipe;
	int  i,j;
	if(0 >= readlink("/proc/self/exe", path, 256)){
		return -1;
	}	
	snprintf(cmd,1024,"addr2line -fCse %s %s", path, addr);
	pipe = popen(cmd, "r");
	if(!pipe) return -1;
	i = fread(output,1,size-1,pipe);	
	pclose(pipe);
	output[i] = '\0';
	for(j=0; j<=i; ++j){ 
		if(output[j] == '\n') output[j] = ' ';	
	}
	return 0;
}

void exception_throw(int32_t code,const char *file,
				const char *func,int32_t line,
				siginfo_t* info)
{
	void*                   bt[64];
	char**                  strings;
	char                    *str1,*str2;
	size_t                  sz;
	int32_t                 i,f;
	int32_t                 sig = 0;
	int32_t 				size = 0;	
	char 					logbuf[MAX_LOG_SIZE],addr[128],buf[1024];
	char 					*ptr = logbuf;		
	int                     len;
	exception_perthd_st		*epst;
	callstack_frame			*call_frame;
	exception_frame			*frame = expstack_top();
	if(frame)
	{
		frame->exception = code;
		frame->line = line;
		frame->is_process = 0;
		if(info)frame->addr = info->si_addr;
		sz = backtrace(bt, 64);
		strings = backtrace_symbols(bt, sz);
		epst = __get_perthread_exception_st();
		if(code == except_segv_fault || code == except_sigbus || code == except_arith) 
			i = 3;
		else
			i = 1;
		for(; i < sz; ++i){
			if(strstr(strings[i],"exception_throw"))
				continue;
			call_frame = get_csf(&epst->csf_pool);
			str1 = strstr(strings[i],"[");
			str2 = strstr(str1,"]");
			
			do{
				if(str1 && str2 && str2 - str1 < 128){
					len = str2 - str1 - 1;
					strncpy(addr,str1+1,len);
					addr[len] = '\0';
					if(0 == addr2line(addr,buf,1024)){
						snprintf(call_frame->info,1024,"%s %s\n",strings[i],buf);
						break;
					}
				}
				snprintf(call_frame->info,1024,"%s\n",strings[i]);
			}while(0);
			
			list_pushback(&frame->call_stack,&call_frame->node);
			if(strstr(strings[i],"main+"))
				break;
		}
		free(strings);
		if(code == except_segv_fault) sig = SIGSEGV;
		else if(code == except_sigbus) sig = SIGBUS;
		else if(code == except_arith) sig = SIGFPE;  
		siglongjmp(frame->jumpbuffer,sig);
	}
	else
	{
		sz = backtrace(bt, 64);
		strings = backtrace_symbols(bt, sz);
		if(code == except_segv_fault)
    		size += snprintf(ptr,MAX_LOG_SIZE," %s [invaild access addr:%p]\n",
    						 exception_description(code),info?info->si_addr:NULL);
		else
    		size += snprintf(ptr,MAX_LOG_SIZE," %s\n",exception_description(code));
		ptr = logbuf + size;	    		    		
 		f = 0;   			
		if(code == except_segv_fault || code == except_sigbus || code == except_arith) 
			i = 3;
		else
			i = 1;
		for(; i < sz; ++i){
			str1 = strstr(strings[i],"[");
			str2 = strstr(str1,"]");
			do{
				if(str1 && str2 && str2 - str1 < 128){
					len = str2 - str1 - 1;
					strncpy(addr,str1+1,len);
					addr[len] = '\0';	
					if(0 == addr2line(addr,buf,1024)){
		        		size += snprintf(ptr,MAX_LOG_SIZE-size,"    % 2d: %s %s\n",++f,strings[i],buf);
		        		break;
					}
				}
        		size += snprintf(ptr,MAX_LOG_SIZE-size,"    % 2d: %s\n",++f,strings[i]);						
			}while(0);
			ptr = logbuf + size;
			if(strstr(strings[i],"main+"))
				break;
		}
		SYS_LOG(LOG_ERROR,"%s",logbuf);
		free(strings);
		exit(0);
	}
}

void   show_call_stack()
{
	void*                   bt[64];
	char**                  strings;
	char                    *str1,*str2;
	size_t                  sz;
	int32_t                 i,f;
	int32_t 				size = 0;	
	char 					logbuf[MAX_LOG_SIZE],addr[128],buf[1024];
	char 					*ptr = logbuf;		
	int                     len;
	sz = backtrace(bt, 64);
	strings = backtrace_symbols(bt, sz);
	ptr = logbuf + size;	    		    		
	f = 0;   			
	for(i = 1; i < sz; ++i){
		str1 = strstr(strings[i],"[");
		str2 = strstr(str1,"]");
		do{
			if(str1 && str2 && str2 - str1 < 128){
				len = str2 - str1 - 1;
				strncpy(addr,str1+1,len);
				addr[len] = '\0';	
				if(0 == addr2line(addr,buf,1024)){
	        		size += snprintf(ptr,MAX_LOG_SIZE-size,"    % 2d: %s %s\n",++f,strings[i],buf);
	        		break;
				}
			}
    		size += snprintf(ptr,MAX_LOG_SIZE-size,"    % 2d: %s\n",++f,strings[i]);						
		}while(0);
		ptr = logbuf + size;
		if(strstr(strings[i],"main+"))
			break;
	}
	SYS_LOG(LOG_ERROR,"\n%s",logbuf);
	free(strings);	
}
