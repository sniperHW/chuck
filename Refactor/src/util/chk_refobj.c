#include <time.h>
#include "util/chk_refobj.h"
#include "util/chk_util.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

chk_refhandle chk_invaild_refhandle;

volatile uint32_t g_ref_counter = 0;

static   time_t   g_start_timestamp = 0;

void chk_refobj_init(chk_refobj *r,void (*dctor)(void*)) {
	time_t   now = time(NULL);
    chk_compare_and_swap(&g_start_timestamp,0,now);
    r->dctor     = dctor;
	r->identity  = (now - g_start_timestamp) << 30;  
    r->identity += chk_atomic_increase_fetch(&g_ref_counter) & 0x3FFFFFF;
	chk_atomic_increase_fetch(&r->refcount);
}

uint32_t chk_refobj_release(chk_refobj *r) {
    volatile uint32_t count;
    uint32_t c;
    struct timespec ts;
    assert(r->refcount > 0);
    if((count = chh_atomic_decrease_fetch(&r->refcount)) == 0) {
        for(c = 0,r->identity = 0;;) {
            if(chk_compare_and_swap(&r->flag,0,1)) break;
            if(++c < 4000) __asm__("pause");
            else {
                c = 0;
                ts.tv_sec = 0;
                ts.tv_nsec = 500000;
                nanosleep(&ts, NULL);
            }
        }
        r->dctor(r);
    }
    return count;
}

chk_refobj *chk_cast2refobj(chk_refhandle h) {
    struct timespec ts;
    uint32_t      c = 0;
    chk_refobj *ptr = NULL;
    chk_refobj *o   = NULL;
    if(chk_unlikely(!h.ptr)) return NULL;
    TRY{
        o = cast(chk_refobj*,h.ptr);
        while(h.identity == o->identity) {
            if(chk_compare_and_swap(&o->flag,0,1)) {
                if(h.identity == o->identity) {
                    if(chk_atomic_fetch_decrease(&o->refcount) > 0)
                        ptr = o;
                    else
                        o->refcount = 0;
                }
                o->flag = 0;
                break;
            }else{                     
                if(++c < 4000) __asm__("pause");
                else {
                    c = 0;
                    ts.tv_sec = 0;
                    ts.tv_nsec = 500000;
                    nanosleep(&ts, NULL);
                }
            }                
        }
    }CATCH_ALL {
        //出现访问异常表示指向对象内存已归还系统
        ptr = NULL;      
    }ENDTRY;
    return ptr; 
}