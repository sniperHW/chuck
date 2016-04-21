#include <string.h>
#include "util/chk_log.h"
#include "util/chk_list.h"
#include "util/chk_atomic.h"
#include "util/chk_time.h"
#include "thread/chk_thread.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

CHK_DEF_LOG(chk_sys_log,CHK_SYSLOG_NAME);
CHK_IMP_LOG(chk_sys_log);

static pthread_once_t 	g_log_key_once      = PTHREAD_ONCE_INIT;
static pid_t          	g_pid               = -1;
static uint32_t         flush_interval      = 1;  //flush every 1 second
int32_t                 g_loglev            = LOG_INFO;
static volatile int32_t stop                = 0;
static int32_t   		lock 				= 0;
static chk_dlist        g_log_file_list;
static chk_thread      *g_log_thd;

#define LOCK() while (__sync_lock_test_and_set(&lock,1)) {}
#define UNLOCK() __sync_lock_release(&lock);

const char *log_lev_str[] = {
	"INFO",
	"DEBUG",
	"WARN",
	"ERROR",
	"CRITICAL",
};

struct chk_logfile {
	chk_dlist_entry   entry;
	char              filename[256];
	FILE             *file;
	uint32_t          total_size;
};

typedef struct {
	chk_list_entry entry;
	int8_t         lev;
	chk_logfile   *_logfile;
	char          *content;
}log_entry;

struct {
	chk_mutex        mtx;
	chk_condition    cond;
	chk_list         private_queue;
	chk_list         share_queue;
	volatile int     wait;	
}logqueue;

static void  write_console(int8_t loglev,char *content) {
	switch(loglev) {
		case LOG_INFO     : printf("%s\n",content); break;
		case LOG_DEBUG    : printf("\033[1;32;40m%s\033[0m\n",content); break;
		case LOG_WARN     : printf("\033[1;33;40m%s\033[0m\n",content); break;
		case LOG_ERROR    : printf("\033[1;35;40m%s\033[0m\n",content); break;
		case LOG_CRITICAL : printf("\033[5;31;40m%s\033[0m\n",content); break;
		default           : break;		
 	}
}

void logqueue_push(log_entry *item) {
	write_console(item->lev,item->content);
	chk_mutex_lock(&logqueue.mtx);
	chk_list_pushback(&logqueue.share_queue,cast(chk_list_entry*,item));
	if(logqueue.wait && chk_list_size(&logqueue.share_queue) == 1) {
		chk_mutex_unlock(&logqueue.mtx);
		chk_condition_signal(&logqueue.cond);
		return;
	}
	chk_mutex_unlock(&logqueue.mtx);
}

log_entry *logqueue_fetch(uint32_t ms) {
	if(chk_list_size(&logqueue.private_queue) > 0)
		return cast(log_entry*,chk_list_pop(&logqueue.private_queue));
	chk_mutex_lock(&logqueue.mtx);
	if(ms > 0) {
		if(chk_list_size(&logqueue.share_queue) == 0) {
			logqueue.wait = 1;
			chk_condition_timedwait(&logqueue.cond,ms);
			logqueue.wait = 0;
		}
	}
	if(chk_list_size(&logqueue.share_queue) > 0)
		chk_list_pushlist(&logqueue.private_queue,&logqueue.share_queue);
	chk_mutex_unlock(&logqueue.mtx);
	return cast(log_entry*,chk_list_pop(&logqueue.private_queue));
}

int32_t chk_log_prefix(char *buf,uint8_t loglev)
{
	struct timespec tv;
	struct tm _tm;
    clock_gettime (CLOCK_REALTIME, &tv);	
	localtime_r(&tv.tv_sec, &_tm);
	return sprintf(buf,"[%8s]%04d-%02d-%02d-%02d:%02d:%02d.%03d[%u]:",
				   log_lev_str[loglev],
				   _tm.tm_year+1900,
				   _tm.tm_mon+1,
				   _tm.tm_mday,
				   _tm.tm_hour,
				   _tm.tm_min,
				   _tm.tm_sec,
				   cast(int32_t,tv.tv_nsec/1000000),
				   cast(uint32_t,chk_thread_current_tid()));
}

static void *log_routine(void *arg) {
	log_entry       *entry;
	chk_logfile     *l;
	chk_dlist_entry *n;	
	int32_t          size;
	char             filename[128],buf[128];
	struct timespec  tv;
	struct tm        _tm;			
	time_t           next_fulsh = time(NULL) + flush_interval;

	for(;;) {
		if((entry = logqueue_fetch(stop?0:100))) {
			if(!entry->_logfile->file || entry->_logfile->total_size > CHK_MAX_LOG_FILE_SIZE) {
				if(entry->_logfile->file) {
					fclose(entry->_logfile->file);
					entry->_logfile->total_size = 0;
				}
				//创建文件
				clock_gettime(CLOCK_REALTIME, &tv);
				localtime_r(&tv.tv_sec, &_tm);
				snprintf(filename,128,"%s[%d]-%04d-%02d-%02d %02d.%02d.%02d.%03d.log",
						 entry->_logfile->filename,
						 getpid(),
					     _tm.tm_year+1900,
					     _tm.tm_mon+1,
					     _tm.tm_mday,
					     _tm.tm_hour,
					     _tm.tm_min,
					     _tm.tm_sec,
					     cast(int32_t,tv.tv_nsec/1000000));
				entry->_logfile->file = fopen(filename,"w+");				
			}
			
			if(entry->_logfile && entry->_logfile->file){
				fprintf(entry->_logfile->file,"%s\n",entry->content);
				entry->_logfile->total_size += strlen(entry->content);
			}

			free(entry->content);
			free(entry);
		}else if(stop) break;

		if(time(NULL) >= next_fulsh) {
			l = NULL;	
			LOCK();
			chk_dlist_foreach(&g_log_file_list,n) {
				l = cast(chk_logfile *,n);
				if(l->file) fflush(l->file);
			}
			UNLOCK();
			next_fulsh = time(NULL) + flush_interval;
		}		
	}

	//向所有打开的日志文件写入"log close success"
	LOCK();
	chk_dlist_foreach(&g_log_file_list,n) {
		l = cast(chk_logfile*,n);
		if(l->file) {
			size = chk_log_prefix(buf,LOG_INFO);
			snprintf(&buf[size],128-size,"log close success");
			fprintf(l->file,"%s\n",buf);
			fflush(l->file);
			fclose(l->file);
			write_console(LOG_INFO,buf);			
		}
	}	
	UNLOCK();
	return NULL;
}

static void on_process_end() {
	if(g_pid == getpid()) {
		stop = 1;
		chk_thread_join(g_log_thd);
	}
}

void _write_log(chk_logfile *l,int32_t loglev,char *content) {
	log_entry *entry = calloc(1,sizeof(*entry));
	entry->_logfile  = l;
	entry->content   = content;
	entry->lev       = loglev;
	logqueue_push(entry);
}
			           
static void log_once_routine() {
	g_pid = getpid();	
	chk_dlist_init(&g_log_file_list);
	chk_list_init(&logqueue.private_queue);
	chk_list_init(&logqueue.share_queue);
	chk_mutex_init(&logqueue.mtx);
	chk_condition_init(&logqueue.cond,&logqueue.mtx);
	g_log_thd = chk_thread_new(log_routine,NULL);	
	atexit(on_process_end);
}

chk_logfile *chk_create_logfile(const char *filename) {
	chk_logfile *l;
	pthread_once(&g_log_key_once,log_once_routine);
	l = calloc(1,sizeof(*l));
	strncpy(l->filename,filename,256);
	LOCK();
	chk_dlist_pushback(&g_log_file_list,cast(chk_dlist_entry*,l));
	UNLOCK();	
	return l;
}

void chk_log(chk_logfile* l,int32_t loglev,char *content) {
	_write_log(l,loglev,content);
}

void chk_syslog(int32_t loglev,char *content) {
	_write_log(CHK_GET_LOGFILE(chk_sys_log),loglev,content);
}