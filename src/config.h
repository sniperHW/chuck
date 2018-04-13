#ifndef CHK_CONFIG_H
#define CHK_CONFIG_H

/*
*  网络IO使用gather方式一次可以发送/接收多个缓冲区
*  MAX_WBAF控制缓冲的最大数量
*/

#define MAX_WBAF             1024

/*
 * 发起一次send系统调用时,网络层在MAX_WBAF限制内会尽可能的
 * 组织更多的缓冲一次发送出去.如果已经组织的缓冲总字节数 >= MAX_SEND_SIZE
 * 将不会继续添加更多的缓冲
*/

#define MAX_SEND_SIZE        1024*64

/*
*  定时器支持的最大超时值(毫秒),如果传入的超时值大于MAX_TIMEOUT
*  超时值将被设置为MAX_TIMEOUT 
*/

#define MAX_TIMEOUT 		 (1000*3600*24-1)

/*
*  定时器对象池的初始大小
*/

#define INIT_FREE_TIMER_SIZE 1024


/*
*  单个日志文件的最大大小,超过这个值将会创建新的日志文件 
*/

#define CHK_MAX_LOG_FILE_SIZE 1024*1024*512


/*
*  系统日志默认文件名前缀
*/

#define CHK_SYSLOG_NAME   "chk_syslog"

/*
*  一条日志的最大字节数 
*/

#define CHK_MAX_LOG_SIZE  4096


/*
*  idle定时器的间隔,在空闲的情况下event_loop每CHK_IDLE_TIMER_TIMEOUT时间触发一次idle回调(如果有设置)
*/

#define CHK_IDLE_TIMER_TIMEOUT 100

#define MAX_LOG_FILE_NAME 256

#define SEND_TIME_OUT 5*1000

#define REDIS_DEFAULT_TIMEOUT 10


#endif