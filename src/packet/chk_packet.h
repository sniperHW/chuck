#ifndef _CHK_PACKET_H
#define _CHK_PACKET_H

#include "util/chk_bytechunk.h"
#include "util/chk_list.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

enum {
	CHK_WPACKET = 1,
	CHK_RPACKET,
	CHK_RAWPACKET,
    CHK_HTTPPACKET,
	CHK_PACKET_END,
};

typedef struct chk_packet chk_packet;


typedef struct  {
    //从当前packet构造一个用于写的packet(共享底层buffer,写的时候触发copy on write)
    chk_packet*      (*construct_write)(chk_packet*);
    //从当前packet构造一个用于读的packet(共享底层buffer)    
    chk_packet*      (*construct_read)(chk_packet*);
    //克隆当前packet(共享底层buffer,写的时候触发copy on write)    
    chk_packet*      (*clone)(chk_packet*);              
    void             (*dctor)(chk_packet*);  
}chk_packet_membfunc;


struct chk_packet {
    chk_list_entry       node;   
    chk_bytechunk       *head;              //数据chunk表头
    uint64_t             sendtime;          //packet被推入发送队列的时间
    uint32_t             len_packet;        //整个数据包的大小         
    uint32_t             spos;              //数据在buffer表头中的起始下标
    uint8_t              type;
    chk_packet_membfunc  membf;
};

static inline void chk_packet_del(chk_packet *p) {
    if(p->membf.dctor) p->membf.dctor(p);
    if(p->head) chk_bytechunk_release(p->head);
    free(p);
}

#define chk_make_writepacket(p)                                           \
    cast(chk_packet*,p)->membf.construct_write(cast(chk_packet*,p))

#define chk_make_readpacket(p)                                            \
    cast(chk_packet*,p)->membf.construct_read(cast(chk_packet*,p))

#define chk_clone_packet(p)                                               \
    cast(chk_packet*,p)->membf.clone(cast(chk_packet*,p)) 


#endif