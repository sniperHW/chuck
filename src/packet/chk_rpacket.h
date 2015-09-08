/*
*  一个协议数据包,与chk_wpacket配对.
*  chk_rpacket用于读,chk_wpacket用于写
*  协议规则如下|CHK_LEN_TYPE|payload|
*  包头的CHK_LEN_TYPE长度的二进制数据表示payload的二进制字节数
*  payload中如果存在变长二进制数据,则数据被以如下方式写入|CHK_LEN_TYPE|data|
*  其中CHK_LEN_TYPE长度的二进制数据表示data的二进制字节数.
*/

#ifndef _CHK_RPACKET_H
#define _CHK_RPACKET_H

#include <stdint.h>
#include "packet/chk_packet.h"
#include "util/chk_endian.h"

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

typedef struct chk_rpacket chk_rpacket;      

struct chk_rpacket {
    chk_packet      base;
    //当前读取的buffer
    chk_bytechunk  *readbuf;
    //readbuf中的下标            
    uint32_t        readpos;
    //剩余未读取的数据量            
    uint32_t        data_remain;        
    /* 数据在packet以buffer链表的形式存储,因此一段变长数据可能会跨域buffer 
    *  当使用者请求的变长数据跨越buffer时,首先将数据全部copy到binbuf中,形成连续数据
    *  再把起始地址返回给使用者.
    */
    chk_bytechunk  *binbuf;
    uint32_t        binpos;
};


//从buffer构造chk_rpacket
chk_rpacket *chk_rpacket_new(chk_bytechunk *b,uint32_t start_pos);


//从packet中读取size字节的数据,会导致读位置及剩余数据的改变
static inline int32_t chk_rpacket_read(chk_rpacket *r,char *out,uint32_t size) {
    uint32_t rsize = size;
    if(size > r->data_remain) return 0;//请求数据大于剩余数据
    r->readbuf      = chk_bytechunk_read(r->readbuf,out,&r->readpos,&rsize);
    r->data_remain -= rsize;
    return 1;
}

//从packet中读取size字节的数据,不会导致读位置及剩余数据的改变(用于窥视)
static inline int32_t chk_rpacket_peek(chk_rpacket *r,char *out,uint32_t size) {
    uint32_t     rsize = size;
    uint32_t     pos   = r->readpos;
    if(size > r->data_remain) return 0;//请求数据大于剩余数据
    chk_bytechunk_read(r->readbuf,out,&pos,&rsize);
}


#define CHK_RPACKET_READ(R,TYPE)                                   ({\
    TYPE __result=0;                                                 \
    chk_rpacket_read(R,(char*)&__result,sizeof(TYPE));               \
    __result;                                                       })

#define CHK_RPACKET_PEEK(R,TYPE)                                   ({\
   TYPE __result=0;                                                  \
   chk_rpacket_read(R,(char*)&__result,sizeof(TYPE));                \
   __result;                                                        })

static inline uint8_t chk_rpacket_readU8(chk_rpacket *r) {
    return CHK_RPACKET_READ(r,uint8_t);
}

static inline uint16_t chk_rpacket_readU16(chk_rpacket *r) {
    return chk_ntoh16(CHK_RPACKET_READ(r,uint16_t));
}

static inline uint32_t chk_rpacket_readU32(chk_rpacket *r) {
    return chk_ntoh32(CHK_RPACKET_READ(r,uint32_t));
}

static inline uint64_t chk_rpacket_readU64(chk_rpacket *r) {
    return chk_ntoh64(CHK_RPACKET_READ(r,uint64_t));
}

static inline double chk_rpacket_readDub(chk_rpacket *r) {
    return CHK_RPACKET_READ(r,double);
}

static inline uint8_t chk_rpacket_peekU8(chk_rpacket *r) {
    return CHK_RPACKET_PEEK(r,uint8_t);
}

static inline uint16_t chk_rpacket_peekU16(chk_rpacket *r) {
    return chk_ntoh16(CHK_RPACKET_PEEK(r,uint16_t));
}

static inline uint32_t chk_rpacket_peekU32(chk_rpacket *r) {
    return chk_ntoh32(CHK_RPACKET_PEEK(r,uint32_t));
}

static inline uint64_t rpacket_peekU64(chk_rpacket *r) {
    return chk_ntoh64(CHK_RPACKET_PEEK(r,uint64_t));
}

static inline double rpacket_peekDub(chk_rpacket *r) {
    return CHK_RPACKET_PEEK(r,double);
}

const char *chk_rpacket_readCStr(chk_rpacket*);

void *chk_rpacket_readBin(chk_rpacket*,uint32_t *len);

#endif    