/*
    Copyright (C) <2015>  <sniperHW@163.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _LOG_H
#define _LOG_H

#include <stdint.h>
#include "comm.h"    
#include "util/singleton.h"

/*
* 一个简单的日志系统
*/

enum{
    LOG_INFO = 0,
    LOG_DEBUG,
    LOG_WARN,	
    LOG_ERROR,
    LOG_CRITICAL
};

#define MAX_FILE_SIZE 1024*1024*256  //日志文件最大大小256MB

extern int32_t g_loglev;

typedef struct logfile logfile;

#define SYSLOG_NAME "syslog"

logfile *create_logfile(const char *filename);


/*  don't use this 3 functions directly
*   used the marcos below instead
*/
void write_log(logfile*,const char *context);

//写入系统日志,默认文件名由SYSLOG_NAME定义
void write_sys_log(const char *content);

int32_t write_prefix(char *buf,uint8_t loglev);

static inline void set_log_lev(int32_t loglev)
{
    if(loglev >= LOG_INFO && loglev <= LOG_CRITICAL)
        g_loglev = loglev;
}

#define  MAX_LOG_SIZE 65535

//日志格式[INFO|ERROR]yyyy-mm-dd-hh:mm:ss.ms:content
#define LOG(LOGFILE,LOGLEV,...)                                                                    \
            do{                                                                                                          \
                if(LOGLEV >= g_loglev){                                                                     \
                    char xx___buf[MAX_LOG_SIZE];                                                     \
                    int32_t size = write_prefix(xx___buf,LOGLEV);                              \
                    snprintf(&xx___buf[size],MAX_LOG_SIZE-size,__VA_ARGS__);      \
                    write_log(LOGFILE,xx___buf);                                                        \
                }                                                                                                          \
            }while(0)

#define SYS_LOG(LOGLEV,...)                                                                          \
            do{                                                                                                         \
                if(LOGLEV >= g_loglev){                                                                    \
                    char xx___buf[MAX_LOG_SIZE];                                                    \
                    int32_t size = write_prefix(xx___buf,LOGLEV);                             \
                    snprintf(&xx___buf[size],MAX_LOG_SIZE-size,__VA_ARGS__);     \
                    write_sys_log(xx___buf);                                                               \
                }                                                                                                         \
            }while(0)


#define DEF_LOG(LOGNAME,LOGFILENAME)\
        typedef struct{  logfile *_logfile;}LOGNAME;\
        static inline LOGNAME *LOGNAME##create_function(){\
        	LOGNAME *tmp = calloc(1,sizeof(*tmp));\
        	tmp->_logfile = create_logfile(LOGFILENAME);\
        	return tmp;\
        }\
        DECLARE_SINGLETON(LOGNAME)

#define IMP_LOG(LOGNAME) IMPLEMENT_SINGLETON(LOGNAME,LOGNAME##create_function,NULL)

#define GET_LOGFILE(LOGNAME) GET_INSTANCE(LOGNAME)->_logfile 

#ifdef _CHUCKLUA

typedef struct lua_State lua_State;

void lua_reglog(lua_State *L);

#endif          


#endif
