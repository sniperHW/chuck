/*
    Copyright (C) <2014>  <sniperHW@163.com>

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
#ifndef _SYSTIME_H
#define _SYSTIME_H
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>  
#include <pthread.h>
#include "comm.h" 

struct _clock
{
    uint64_t last_tsc;
    uint64_t last_time;
};

static __thread struct _clock *__t_clock = NULL;

static inline void _clock_gettime_boot(struct timespec *ts){
        if(unlikely(!ts)) return;
#ifdef _LINUX
        clock_gettime(CLOCK_BOOTTIME, ts);
#elif _BSD
        clock_gettime(CLOCK_UPTIME, ts); 
#else
        #error "un support platform!"         
#endif 
}

#define NN_CLOCK_PRECISION 1000000

static inline uint64_t _clock_rdtsc ()
{
#if (defined _MSC_VER && (defined _M_IX86 || defined _M_X64))
    return __rdtsc ();
#elif (defined __GNUC__ && (defined __i386__ || defined __x86_64__))
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
    return (uint64_t) high << 32 | low;
#elif (defined __SUNPRO_CC && (__SUNPRO_CC >= 0x5100) && (defined __i386 || \
    defined __amd64 || defined __x86_64))
    union {
        uint64_t u64val;
        uint32_t u32val [2];
    } tsc;
    asm("rdtsc" : "=a" (tsc.u32val [0]), "=d" (tsc.u32val [1]));
    return tsc.u64val;
#else
    /*  RDTSC is not available. */
    return 0;
#endif
}

static inline uint64_t _clock_time()
{
    struct timespec tv;
    _clock_gettime_boot(&tv);
    return tv.tv_sec * (uint64_t) 1000 + tv.tv_nsec / 1000000;
}

static void __clock_child_at_fork(){
    if(__t_clock){        
        free(__t_clock);
        __t_clock = NULL;
    }
}

static inline void _clock_init()
{
    __t_clock->last_tsc = _clock_rdtsc();
    __t_clock->last_time = _clock_time();
    pthread_atfork(NULL,NULL,__clock_child_at_fork);
}

static inline struct _clock* get_thread_clock()
{
    if(!__t_clock){
        __t_clock = calloc(1,sizeof(*__t_clock));
        _clock_init();
    }
    return __t_clock;
}

  
static inline uint64_t systick64()
{
    uint64_t tsc = _clock_rdtsc();
    if (!tsc)
        return _clock_time();

    struct _clock *c = get_thread_clock();

    /*  If tsc haven't jumped back or run away too far, we can use the cached
        time value. */
    if (likely(tsc - c->last_tsc <= (NN_CLOCK_PRECISION / 2) && tsc >= c->last_tsc))
        return c->last_time;

    /*  It's a long time since we've last measured the time. We'll do a new
        measurement now. */
    c->last_tsc = tsc;
    c->last_time = _clock_time();
    return c->last_time;
}

#define systick32() ((uint32_t)systick64())     

#define SLEEPMS(MS) (usleep((MS)*1000))


/*
static inline char *GetCurrentTimeStr(char *buf)
{
	time_t _now = time(NULL);
#ifdef _WIN
	struct tm *_tm;
	_tm = localtime(&_now);
	snprintf(buf,22,"[%04d-%02d-%02d %02d:%02d:%02d]",_tm->tm_year+1900,_tm->tm_mon+1,_tm->tm_mday,_tm->tm_hour,_tm->tm_min,_tm->tm_sec);
#else
	struct tm _tm;
	localtime_r(&_now, &_tm);
	snprintf(buf,22,"[%04d-%02d-%02d %02d:%02d:%02d]",_tm.tm_year+1900,_tm.tm_mon+1,_tm.tm_mday,_tm.tm_hour,_tm.tm_min,_tm.tm_sec);
#endif
	return buf;
}*/
#endif
