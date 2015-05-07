#include "util/refobj.h"
#include "util/time.h"
#include "comm.h"

volatile uint32_t g_ref_counter = 0;

void 
refobj_init(refobj *r,void (*dctor)(void*))
{
	r->dctor = dctor;
	r->high32 = systick32();
	r->low32  = (uint32_t)(ATOMIC_INCREASE_FETCH(&g_ref_counter));
	ATOMIC_INCREASE_FETCH(&r->refcount);
}

uint32_t 
refobj_dec(refobj *r)
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

refobj*
cast2refobj(refhandle h)
{
    refobj *ptr = NULL;
    if(unlikely(!h.ptr)) return NULL;
    TRY{
        refobj *o = (refobj*)h.ptr;
        uint32_t c = 0;
    	struct timespec ts;
        while(h.identity == o->identity){
            if(COMPARE_AND_SWAP(&o->flag,0,1)){
                if(h.identity == o->identity){
                    if(ATOMIC_FETCH_DECREASE(&o->refcount) > 0)
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