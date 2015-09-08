#ifndef _CHK_WPACKET_H
#define _CHK_WPACKET_H

#include <stdarg.h>    
#include "packet/chk_packet.h"
#include "util/chk_endian.h"
#include "util/chk_log.h"


#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

#ifndef CHK_LEN_TYPE
#   define CHK_LEN_TYPE uint16_t
#endif


#if CHK_LEN_TYPE == uint16_t 
#   define chk_ntoh  chk_ntoh16
#   define chk_hton  chk_hton16
#elif CHK_LEN_TYPE == uint32_t
#   define chk_ntoh  chk_ntoh32
#   define chk_hton  chk_hton32
#else
#   error "un support CHK_LEN_TYPE!"
#endif

typedef struct chk_wpacket chk_wpacket;

struct chk_wpacket {
    chk_packet      base;
    chk_bytechunk  *wbuf;            
    uint32_t        wpos;    
    CHK_LEN_TYPE   *len;  //指向长度字段
};

chk_wpacket *chk_wpacket_new(uint32_t size);

static inline void chk_wpacket_copy_on_write(chk_wpacket *w,uint32_t newsize) {
    chk_bytechunk *o    = cast(chk_packet*,w)->head;
    chk_bytechunk *n    = chk_bytechunk_new(NULL,newsize);
    uint32_t      size  = cast(chk_packet*,w)->len_packet;
    uint32_t      spos  = cast(chk_packet*,w)->spos;
    w->wbuf = cast(chk_packet*,w)->head = n;
    w->len  = cast(CHK_LEN_TYPE*,n->data); 
    chk_bytechunk_read(o,n->data,&spos,&size);
}

static inline void chk_wpacket_expand(chk_wpacket *w,uint32_t size) {
    w->wbuf->next = chk_bytechunk_new(NULL,chk_size_of_pow2(size));
}

static inline void chk_wpacket_write(chk_wpacket *w,char *in,uint32_t size) {
    CHK_LEN_TYPE packet_len = cast(chk_packet*,w)->len_packet;
    CHK_LEN_TYPE newsize    = packet_len + size;
    if(newsize < packet_len) {
        //log
        return;
    }
    //执行写时拷贝
    if(!w->len) chk_wpacket_copy_on_write(w,newsize);
    //空间不足,扩张
    if(w->wbuf->cap - w->wpos < size) chk_wpacket_expand(w,newsize);
    w->wbuf = chk_bytechunk_write(w->wbuf,in,&w->wpos,&size);
    cast(chk_packet*,w)->len_packet = newsize;
    *w->len = chk_hton(newsize - sizeof(CHK_LEN_TYPE));
}


static inline void chk_wpacket_writeU8(chk_wpacket *w,uint8_t value) {  
    chk_wpacket_write(w,cast(char*,&value),sizeof(value));
}

static inline void chk_wpacket_writeU16(chk_wpacket *w,uint16_t value) {
    value = chk_hton16(value);
    chk_wpacket_write(w,cast(char*,&value),sizeof(value));        
}

static inline void chk_wpacket_writeU32(chk_wpacket *w,uint32_t value) {   
    value = chk_hton32(value);
    chk_wpacket_write(w,cast(char*,&value),sizeof(value));
}

static inline void chk_wpacket_writeU64(chk_wpacket *w,uint64_t value) {   
    value = chk_hton64(value);
    chk_wpacket_write(w,cast(char*,&value),sizeof(value));
}

static inline void chk_wpacket_writeDub(chk_wpacket *w,uint64_t value) {   
    chk_wpacket_write(w,cast(char*,&value),sizeof(value));
}

static inline void chk_wpacket_writeBin(chk_wpacket *w,const void *value,
                                        uint32_t size) {
#if CHK_LEN_TYPE == uint16_t
    chk_wpacket_writeU16(w,size);
#else
    chk_wpacket_writeU32(w,size);
#endif    
    chk_wpacket_write(w,cast(char*,value),size);
}

static inline void chk_wpacket_writeCtr(chk_wpacket *w ,const char *value) {
    chk_wpacket_writeBin(w,value,strlen(value)+1);
}

#endif  