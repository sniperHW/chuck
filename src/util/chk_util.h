#ifndef _CHK_UTIL_H
#define _CHK_UTIL_H

#include <stdint.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)\
    ({ long int __result;\
    do __result = (long int)(expression);\
    while(__result == -1L&& errno == EINTR);\
    __result;})
#endif


#define MAX_UINT32 0xffffffff
#define MAX_UINT16 0xffff
#define chk_likely(x)   __builtin_expect(!!(x), 1)  
#define chk_unlikely(x) __builtin_expect(!!(x), 0)

#ifndef MAX
#define MAX(L,R) (L) > (R) ? (L) : (R)    
#endif

#ifndef MIN
#define MIN(L,R) (L) > (R) ? (R) : (L)    
#endif    

static inline int32_t chk_is_pow2(uint32_t size) {
    return !(size&(size-1));
}

static inline uint32_t chk_size_of_pow2(uint32_t size) {
    if(chk_is_pow2(size)) return size;
    size = size-1;
    size = size | (size>>1);
    size = size | (size>>2);
    size = size | (size>>4);
    size = size | (size>>8);
    size = size | (size>>16);
    return size + 1;
}

static inline uint8_t chk_get_pow2(uint32_t size) {
    uint8_t pow2 = 0;
    if(!chk_is_pow2(size)) size = (size << 1);
    while(size > 1) {
        pow2++;
        size = size >> 1;
    }
    return pow2;
}

static inline uint32_t chk_align_size(uint32_t size,uint32_t align) {
    uint32_t mod;
    align = chk_size_of_pow2(align);
    if(align < 4) align = 4;
    mod = size % align;
    if(mod == 0) return size;
    else return (size/align + 1) * align;
}

//create a fdpairs,fdpairs[0] for read,fdpairs[1] for write
int32_t chk_create_notify_channel(int32_t fdpairs[2]);

void    chk_close_notify_channel(int32_t fdpairs[2]);

#endif