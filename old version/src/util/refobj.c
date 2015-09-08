#include "util/refobj.h"
#include "comm.h"

volatile uint32_t g_ref_counter = 0;

static   time_t   g_start_timestamp = 0;

void refobj_init(refobj *r,void (*dctor)(void*))
{
    time_t   now = time(NULL);
    COMPARE_AND_SWAP(&g_start_timestamp,0,now); 
	r->dctor     = dctor;
    r->identity  = (now - g_start_timestamp) << 30;  
    r->identity += ATOMIC_INCREASE_FETCH(&g_ref_counter) & 0x3FFFFFF;
	ATOMIC_INCREASE_FETCH(&r->refcount);
}

uint32_t refobj_dec(refobj *r)
{
    volatile uint32_t count;
    uint32_t c;
    struct timespec ts;
    assert(r->refcount > 0);
    if((count = ATOMIC_DECREASE_FETCH(&r->refcount)) == 0){
        for(c = 0,r->identity = 0;;){
            if(COMPARE_AND_SWAP(&r->flag,0,1))
                break;
            if(++c < 4000){
                __asm__("pause");
            }else{
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

refobj *cast2refobj(refhandle h)
{
    uint32_t c = 0;
    struct timespec ts;
    refobj *ptr = NULL;
    refobj *o   = NULL;
    if(unlikely(!h.ptr)) return NULL;
    TRY{
        o = (refobj*)h.ptr;
        while(h.identity == o->identity){
            if(COMPARE_AND_SWAP(&o->flag,0,1)){
                if(h.identity == o->identity){
                    if(ATOMIC_FETCH_INCREASE(&o->refcount) > 0)
                        ptr = o;
                    else
                        o->refcount = 0;
                }
                o->flag = 0;
                break;
            }else{                     
                if(++c < 4000){
                    __asm__("pause");
                }else{
                    c = 0;
                    ts.tv_sec = 0;
                    ts.tv_nsec = 500000;
                    nanosleep(&ts, NULL);
                }
            }                
        }
    }CATCH_ALL{
            ptr = NULL;      
    }ENDTRY;
    return ptr; 
}