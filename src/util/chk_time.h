#ifndef _CHK_TIME_H
#define _CHK_TIME_H

#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>  
#include <pthread.h>
#include "util/chk_util.h"

#ifdef _MACH
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
#define CLOCK_MONOTONIC_RAW 0
static inline int clock_gettime(int clk_id, struct timespec *t){
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return 0;
}
#endif


struct _clock {
    uint64_t last_tsc;
    uint64_t last_time;
};

static __thread struct _clock *__t_clock = NULL;

static inline void _clock_gettime_boot(struct timespec *ts) {
    if(chk_likely(ts)) clock_gettime(CLOCK_MONOTONIC_RAW, ts);
}

#define NN_CLOCK_PRECISION 1000000

static inline uint64_t _clock_rdtsc() {
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

static inline uint64_t _clock_time() {
    struct timespec tv;
    _clock_gettime_boot(&tv);
    return tv.tv_sec * (uint64_t) 1000 + tv.tv_nsec / 1000000;
}

static void __clock_child_at_fork() {
    if(__t_clock){        
        free(__t_clock);
        __t_clock = NULL;
    }
}

static inline void _clock_init() {
    __t_clock->last_tsc = _clock_rdtsc();
    __t_clock->last_time = _clock_time();
    pthread_atfork(NULL,NULL,__clock_child_at_fork);
}

static inline struct _clock *get_thread_clock() {
    if(!__t_clock){
        __t_clock = calloc(1,sizeof(*__t_clock));
        _clock_init();
    }
    return __t_clock;
}

  
static inline uint64_t chk_systick64() {
    uint64_t tsc = _clock_rdtsc();
    if (!tsc)
        return _clock_time();

    struct _clock *c = get_thread_clock();

    /*  If tsc haven't jumped back or run away too far, we can use the cached
        time value. */
    if (chk_likely(tsc - c->last_tsc <= (NN_CLOCK_PRECISION / 2) && tsc >= c->last_tsc))
        return c->last_time;

    /*  It's a long time since we've last measured the time. We'll do a new
        measurement now. */
    c->last_tsc = tsc;
    c->last_time = _clock_time();
    return c->last_time;
}

static inline uint32_t chk_accurate_tick64(){
    return _clock_time();
}

#define chk_systick     chk_systick64

#define chk_systick32() ((uint32_t)chk_systick64())     

#define chk_sleepms(MS) (usleep((MS)*1000))

#endif
