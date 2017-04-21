#ifndef _CHK_BUFFER_READER_H
#define _CHK_BUFFER_READER_H

#include "util/chk_bytechunk.h"

typedef struct {
	chk_bytebuffer *buff;
    chk_bytechunk  *cur;       
    uint32_t        readpos;           
    uint32_t        data_remain;
    uint32_t        total_packet_size;        
}packet_reader;

int32_t packet_reader_init(packet_reader *reader,chk_bytebuffer *buff);

int32_t reader_read(packet_reader *r,char *out,uint32_t size);

#endif