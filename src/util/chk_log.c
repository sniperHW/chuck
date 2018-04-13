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

char    g_syslog_file_prefix[MAX_LOG_FILE_NAME + 1] = {0};
char    g_log_dir[MAX_LOG_FILE_NAME + 1] = "./log";

static pthread_once_t 	g_log_key_once      = PTHREAD_ONCE_INIT;
static pid_t          	g_pid               = -1;
static uint32_t         flush_interval      = 1;  //flush every 1 second
int32_t                 g_loglev            = LOG_TRACE;
static volatile int32_t stop                = 0;
static int32_t   		lock 				= 0;
static chk_dlist        g_log_file_list;
static chk_thread      *g_log_thd;

#define LOCK() while (__sync_lock_test_and_set(&lock,1)) {}
#define UNLOCK() __sync_lock_release(&lock);

const char *log_lev_str[] = {
	"TRACE",
	"DEBUG",	
	"INFO",
	"WARN",
	"ERROR",
	"CRITICAL",
};

struct chk_logfile {
	chk_dlist_entry   entry;
	char              filename[MAX_LOG_FILE_NAME + 1];//文件名
	char              path[MAX_LOG_FILE_NAME + 1];    //所在目录
	FILE             *file;
	struct tm         tm;
	uint32_t          total_size;
};

typedef struct {
	chk_list_entry entry;
	int8_t         lev;
	chk_logfile   *logfile;
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
		case LOG_TRACE    : printf("%s\n",content); break;
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
		strncpy(g_log_dir,log_dir,MAX_LOG_FILE_NAME);
		g_log_dir[MAX_LOG_FILE_NAME] = 0;
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
		strncpy(g_syslog_file_prefix,prefix,MAX_LOG_FILE_NAME);
		g_syslog_file_prefix[MAX_LOG_FILE_NAME] = 0;
	}
}

void logqueue_push(log_entry *item) {
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
	struct tm tm;
    chk_clock_real(&tv);
    chk_localtime(&tm);
	return sprintf(buf,"[%8s]%04d-%02d-%02d-%02d:%02d:%02d.%03d[%u]:",
				   log_lev_str[loglev],
				   tm.tm_year+1900,
				   tm.tm_mon+1,
				   tm.tm_mday,
				   tm.tm_hour,
				   tm.tm_min,
				   tm.tm_sec,
				   cast(int32_t,tv.tv_nsec/1000000),
				   cast(uint32_t,chk_thread_current_tid()));
}

int32_t chk_log_prefix_detail(char *buf,uint8_t loglev,const char *function,const char *file,int32_t line) {
	struct timespec tv;
	struct tm tm;
    chk_clock_real(&tv);
    chk_localtime(&tm);
	return sprintf(buf,"[%8s]%04d-%02d-%02d-%02d:%02d:%02d.%03d[%u] %s():%s:%d:",
				   log_lev_str[loglev],
				   tm.tm_year+1900,
				   tm.tm_mon+1,
				   tm.tm_mday,
				   tm.tm_hour,
				   tm.tm_min,
				   tm.tm_sec,
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

int need_create_logfile(log_entry *entry,struct tm *tm) {
	if(!entry->logfile->file) {
		return 1;
	}

	if(entry->logfile->total_size > CHK_MAX_LOG_FILE_SIZE) {
		return 1;
	}

	if(entry->logfile->tm.tm_year != tm->tm_year  ||
	   entry->logfile->tm.tm_mon  != tm->tm_mon   ||
	   entry->logfile->tm.tm_mday != tm->tm_mday) {
		return 1;
	}

	return 0;
}

static void close_and_rename(chk_logfile *logfile) {
	char oldname[MAX_LOG_FILE_NAME + 1];
	char newname[MAX_LOG_FILE_NAME + 1];	
	if(logfile->file) {
		fflush(logfile->file);
		fclose(logfile->file);
		snprintf(oldname,MAX_LOG_FILE_NAME,"%s/%s[%d].log",logfile->path,logfile->filename,getpid());
		oldname[MAX_LOG_FILE_NAME] = 0;		
		snprintf(newname,MAX_LOG_FILE_NAME,"%s/%s[%d]-%02d.%02d.%02d.log",
			logfile->path,
			logfile->filename,
			getpid(),
			logfile->tm.tm_hour,
			logfile->tm.tm_min,
			logfile->tm.tm_sec			
		);
		newname[MAX_LOG_FILE_NAME] = 0;
		rename(oldname,newname);	
	}
}

static void create_os_file(log_entry *entry,struct tm *tm) {
	char filename[MAX_LOG_FILE_NAME + 1];
	chk_logfile *logfile = entry->logfile;
	snprintf(logfile->path,MAX_LOG_FILE_NAME,"%s/%04d-%02d-%02d",g_log_dir,(*tm).tm_year+1900,(*tm).tm_mon+1,(*tm).tm_mday);
	logfile->path[MAX_LOG_FILE_NAME] = 0;
	if(0 == create_log_dir(logfile->path)) {
		snprintf(filename,MAX_LOG_FILE_NAME,"%s/%s[%d].log",logfile->path,logfile->filename,getpid());
		filename[MAX_LOG_FILE_NAME] = 0;
		entry->logfile->file = fopen(filename,"w+");
		entry->logfile->tm = *tm;					
	}
}

static void *log_routine(void *arg) {
	log_entry       *entry;
	chk_logfile     *l;
	chk_dlist_entry *n;	
	struct tm        tm;			
	time_t           next_fulsh = time(NULL) + flush_interval;

	for(;;) {
		if((entry = logqueue_fetch(stop?0:100))) {
			chk_localtime(&tm);
			if(1 == need_create_logfile(entry,&tm)) {
				if(entry->logfile->file) {
					close_and_rename(entry->logfile);
					entry->logfile->file = NULL;
					entry->logfile->total_size = 0;
				}
				//创建文件
				create_os_file(entry,&tm);	
			}
			write_console(entry->lev,entry->content);
			if(entry->logfile->file){
				fprintf(entry->logfile->file,"%s\n",entry->content);
				entry->logfile->total_size += strlen(entry->content);
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

	LOCK();
	chk_dlist_foreach(&g_log_file_list,n) {
		l = cast(chk_logfile*,n);
		close_and_rename(l);
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
	entry->logfile  = l;
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
	strncpy(l->filename,filename,MAX_LOG_FILE_NAME);
	l->filename[MAX_LOG_FILE_NAME] = 0;
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