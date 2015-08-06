#ifndef CHK_REFOBJ_H
#define CHK_REFOBJ_H

/*
*   线程安全的引用计数对象
*   使用chk_get_refhandle获得对象的handle供任意线程储存与使用
*   通过chk_cast2refobj将handle转换为chk_refobj指针.如果返回非NULL
*   则可以安全使用对象,使用完毕后使用chk_refobj_release释放引用
*
*   该类主要用于跨线程的对象生命周期处理.如无此需求请勿使用
*/

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <assert.h> 
#include "util/chk_atomic.h"
#include "util/chk_exception.h" 


typedef struct chk_refobj    chk_refobj;

typedef struct chk_refhandle chk_refhandle;
  
struct chk_refobj {         
    volatile uint32_t refcount;
    uint32_t          pad1;             
    volatile uint32_t flag;             
    uint32_t          pad2;             
    union{                              
        struct{                         
            volatile uint32_t low32;    
            volatile uint32_t high32;   
        };                              
        volatile uint64_t identity;     
    };                                  
    void (*dctor)(void*);
};

struct chk_refhandle {
    union{  
        struct{ 
            uint64_t      identity;    
            chk_refobj   *ptr;
        };
        uint32_t _data[4];
    };
};

extern chk_refhandle chk_invaild_refhandle;

void        chk_refobj_init(chk_refobj *r,void (*dctor)(void*));

uint32_t    chk_refobj_release(chk_refobj *r);

chk_refobj *chk_cast2refobj(chk_refhandle h);


static inline uint32_t chk_refobj_retain(chk_refobj *r) {
    return chk_atomic_increase_fetch(&r->refcount);
}

static inline int32_t chk_is_vaild_refhandle(chk_refhandle h) {
    return memcmp(&h,&chk_invaild_refhandle,sizeof(chk_invaild_refhandle)) != 0;
}

static inline chk_refhandle chk_get_refhandle(chk_refobj *o) {
    return (chk_refhandle){.identity=o->identity,.ptr=o}; 
} 

#endif
