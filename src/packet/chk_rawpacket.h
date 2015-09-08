/*
*   原始二进制字数据包
*/

#ifndef _CHK_RAWPACKET_H
#define _CHK_RAWPACKET_H

#include "packet/chk_packet.h"

typedef struct chk_rawpacket chk_rawpacket;

struct chk_rawpacket {
    chk_packet      base;
};

//用buff构造一个chk_rawpacket,buff引用计数会被增加
chk_rawpacket *chk_rawpacket_new(chk_bytechunk *buff,uint32_t spos,uint32_t size);  

//返回rawpacket中数据的指针
void          *chk_rawpacket_data(chk_rawpacket *,uint32_t *size);

#endif