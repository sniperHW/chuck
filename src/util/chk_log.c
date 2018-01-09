#include <string.h>
#include "util/chk_log.h"
#include "util/chk_list.h"
#include "util/chk_atomic.h"
#include "util/chk_time.h"
#include "thread/chk_thread.h"
#include "sys/stat.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

CHK_DEF_LOG(chk_sys_log,CHK_SYSLOG_NAME);
CHK_IMP_LOG(chk_sys_log);

char    g_syslog_file_prefix[MAX_LOG_FILE_NAME] = {0};
char    g_log_dir[MAX_LOG_FILE_NAME] = "./log";

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
	char              filename[MAX_LOG_FILE_NAME];
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

void chk_set_log_dir(const char *log_dir){
	if(log_dir) {
		strncpy(g_log_dir,log_dir,MAX_LOG_FILE_NAME-1);
		g_log_dir[MAX_LOG_FILE_NAME-1] = 0;
	}
}

void chk_set_loglev(int32_t loglev){
	g_loglev = loglev;
}

int32_t chk_current_loglev() {
	return g_loglev;
}

const char *chk_get_syslog_file_prefix() {
	return g_syslog_file_prefix;
}

void chk_set_syslog_file_prefix(const char *prefix){
	if(prefix)	{
		strncpy(g_syslog_file_prefix,prefix,MAX_LOG_FILE_NAME-1);
		g_syslog_file_prefix[MAX_LOG_FILE_NAME - 1] = 0;
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

int32_t chk_log_prefix(char *buf,uint8_t loglev) {
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

int32_t chk_log_prefix_detail(char *buf,uint8_t loglev,const char *function,const char *file,int32_t line) {
	struct timespec tv;
	struct tm _tm;
    clock_gettime (CLOCK_REALTIME, &tv);	
	localtime_r(&tv.tv_sec, &_tm);
	return sprintf(buf,"[%8s]%04d-%02d-%02d-%02d:%02d:%02d.%03d[%u] %s():%s:%d:",
				   log_lev_str[loglev],
				   _tm.tm_year+1900,
				   _tm.tm_mon+1,
				   _tm.tm_mday,
				   _tm.tm_hour,
				   _tm.tm_min,
				   _tm.tm_sec,
				   cast(int32_t,tv.tv_nsec/1000000),
				   cast(uint32_t,chk_thread_current_tid()),
				   function,
				   file,
				   line);	
}

static int32_t create_log_dir(const char *log_dir) {
	size_t first_;
	if(log_dir[0] == '.') {
		first_ = 2;
	}else {
		first_ = 1;
	}
	size_t len = strlen(log_dir);
	size_t i = 0;
	size_t c = 0;
	char buf[MAX_LOG_FILE_NAME];
	for( ;i < len; ++i){
		if(log_dir[i] == '/') {
			if(++c >= first_) {
				strncpy(buf,log_dir,i);
				buf[i] = 0;
				int ret = mkdir(buf,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
				if(!(ret == 0 || errno == EEXIST)){
					return errno;
				}
			}
		}
	}
	strncpy(buf,log_dir,len);
	buf[len] = 0;
	int ret = mkdir(buf,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if(!(ret == 0 || errno == EEXIST)){
		return errno;
	} else {
		return 0;
	}
}

static void *log_routine(void *arg) {
	log_entry       *entry;
	chk_logfile     *l;
	chk_dlist_entry *n;	
	int32_t          size;
	char             filename[MAX_LOG_FILE_NAME] = {0};
	char             logdir[MAX_LOG_FILE_NAME] = {0};
	char             buf[128] = {0};
	struct timespec  tv;
	struct tm        _tm;			
	time_t           next_fulsh = time(NULL) + flush_interval;
	int32_t          ret;

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
				snprintf(logdir,sizeof(logdir),"%s/%04d-%02d-%02d",g_log_dir,_tm.tm_year+1900,_tm.tm_mon+1,_tm.tm_mday);
				ret = create_log_dir(logdir);
				if(0 == ret) {
					snprintf(filename,sizeof(filename) - 1,"%s/%s[%d]-%04d-%02d-%02d %02d.%02d.%02d.%03d.log",
							 logdir,
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
				} else {
					printf("create log dir failed:%d\n",ret);
					exit(0);
				}		
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
			snprintf(&buf[size],sizeof(buf)-size-1,"log close success");
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
	if(!entry) free(content);
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
	if(!l) return NULL;
	strncpy(l->filename,filename,sizeof(l->filename) - 1);
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