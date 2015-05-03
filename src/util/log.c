#include <string.h>
#include "util/log.h"
#include "util/list.h"
#include "thread/thread.h"
//#include "atomic.h"

static pthread_once_t 	g_log_key_once = PTHREAD_ONCE_INIT;
static mutex           *g_mtx_log_file_list = NULL;
static list             g_log_file_list;
static thread    	   *g_log_thd = NULL;
static pid_t          	g_pid = -1;
static uint32_t         flush_interval = 5;  //flush every 5 second
static volatile int32_t stop = 0;


const char *log_lev_str[] = {
	"INFO",
	"ERROR"
};

typedef struct logfile{
	listnode node;
	char     filename[256];
	FILE    *file;
	uint32_t total_size;
}logfile;

struct log_item{
	listnode node;
	logfile *_logfile;
	char     content[0];
};

struct{
	mutex       *mtx;
	condition   *cond;
	list         private_queue;
	list         share_queue;
	volatile int wait;	
}logqueue;


void logqueue_push(struct log_item *item){
	mutex_lock(logqueue.mtx);
	list_pushback(&logqueue.share_queue,(listnode*)item);
	if(logqueue.wait && list_size(&logqueue.share_queue) == 1){
		mutex_unlock(logqueue.mtx);
		condition_signal(logqueue.cond);
		return;
	}
	mutex_unlock(logqueue.mtx);
}

struct log_item *logqueue_fetch(uint32_t ms){
	if(list_size(&logqueue.private_queue) > 0)
		return (struct log_item*)list_pop(&logqueue.private_queue);
	mutex_lock(logqueue.mtx);
	if(ms > 0){
		if(list_size(&logqueue.share_queue) == 0){
			logqueue.wait = 1;
			condition_timedwait(logqueue.cond,ms);
			logqueue.wait = 0;
		}
	}
	if(list_size(&logqueue.share_queue) > 0){
		list_pushlist(&logqueue.private_queue,&logqueue.share_queue);
	}
	mutex_unlock(logqueue.mtx);
	return (struct log_item*)list_pop(&logqueue.private_queue);
}


DEF_LOG(sys_log,SYSLOG_NAME);
IMP_LOG(sys_log);

int32_t write_prefix(char *buf,uint8_t loglev)
{
	struct timespec tv;
    clock_gettime (CLOCK_REALTIME, &tv);
	struct tm _tm;
	localtime_r(&tv.tv_sec, &_tm);
	return sprintf(buf,"[%s]%04d-%02d-%02d-%02d:%02d:%02d.%03d[%x]:",
				   log_lev_str[loglev],
				   _tm.tm_year+1900,
				   _tm.tm_mon+1,
				   _tm.tm_mday,
				   _tm.tm_hour,
				   _tm.tm_min,
				   _tm.tm_sec,
				   (int32_t)tv.tv_nsec/1000000,
				   (uint32_t)pthread_self());
}

static void* log_routine(void *arg){
	g_pid = getpid();
	time_t next_fulsh = time(NULL) + flush_interval;
	struct log_item *item;
	while(1){
		item = logqueue_fetch(stop?0:100);
		if(item){
			if(item->_logfile->file == NULL || item->_logfile->total_size > MAX_FILE_SIZE)
			{
				if(item->_logfile->total_size){
					fclose(item->_logfile->file);
					item->_logfile->total_size = 0;
				}
				//还没创建文件
				char filename[128];
				struct timespec tv;
				clock_gettime(CLOCK_REALTIME, &tv);
				struct tm _tm;
				localtime_r(&tv.tv_sec, &_tm);
				snprintf(filename,128,"%s[%d]-%04d-%02d-%02d %02d.%02d.%02d.%03d.log",
						 item->_logfile->filename,
						 getpid(),
					     _tm.tm_year+1900,
					     _tm.tm_mon+1,
					     _tm.tm_mday,
					     _tm.tm_hour,
					     _tm.tm_min,
					     _tm.tm_sec,
					     (int32_t)tv.tv_nsec/1000000);
				item->_logfile->file = fopen(filename,"w+");
			}
			if(item->_logfile->file){
				fprintf(item->_logfile->file,"%s\n",item->content);
				item->_logfile->total_size += strlen(item->content);
			}
			free(item);			
		}else{
			if(stop)
				break;
		}

		if(time(NULL) >= next_fulsh){
			struct logfile *l = NULL;	
			mutex_lock(g_mtx_log_file_list);
			l = (struct logfile*)list_begin(&g_log_file_list);
			while(l != NULL){
				if(l->file) fflush(l->file);
				l = (struct logfile*)((listnode*)l)->next;
			}
			mutex_unlock(g_mtx_log_file_list); 
			next_fulsh = time(NULL) + flush_interval;
		}

	}

	//向所有打开的日志文件写入"log close success"
	struct logfile *l = NULL;
	char buf[128];
	mutex_lock(g_mtx_log_file_list);
	while((l = (logfile*)list_pop(&g_log_file_list)) != NULL)
	{
		if(l->file){
			int32_t size = write_prefix(buf,LOG_INFO);
			snprintf(&buf[size],128-size,"log close success");
			fprintf(l->file,"%s\n",buf);
			fflush(l->file);
		}
	}	
	mutex_unlock(g_mtx_log_file_list);
	return NULL;
}

static void on_process_end()
{
	if(g_pid == getpid()){
		if(g_log_thd){
			stop = 1;
			thread_del(g_log_thd);
		}
	}
}

void _write_log(logfile *l,const char *content)
{
	uint32_t content_len = strlen(content)+1;
	struct log_item *item = calloc(1,sizeof(*item) + content_len);
	item->_logfile = l;
	strncpy(item->content,content,content_len);	
	logqueue_push(item);
}
			           
static void log_once_routine(){
	list_init(&g_log_file_list);
	g_mtx_log_file_list = mutex_new();
	list_init(&logqueue.private_queue);
	list_init(&logqueue.share_queue);
	logqueue.mtx = mutex_new();
	logqueue.cond = condition_new(logqueue.mtx);
	g_log_thd = thread_new(JOINABLE|WAITRUN,log_routine,NULL);	
	atexit(on_process_end);
}

logfile *create_logfile(const char *filename)
{
	pthread_once(&g_log_key_once,log_once_routine);
	logfile *l = calloc(1,sizeof(*l));
	strncpy(l->filename,filename,256);
	mutex_lock(g_mtx_log_file_list);
	list_pushback(&g_log_file_list,(listnode*)l);
	mutex_unlock(g_mtx_log_file_list);	
	return l;
}


void write_log(logfile* l,const char *content){
	_write_log(l,content);
}

void write_sys_log(const char *content){
	_write_log(GET_LOGFILE(sys_log),content);
}
