#ifndef _HTTPPACKET_H
#define _HTTPPACKET_H
#include <stdarg.h>
#include "comm.h"    
#include "packet/packet.h"
#include "mem/allocator.h"
#include "util/endian.h"

typedef struct st_header{
	listnode node;
	size_t   field;
	size_t   value;
}st_header;

typedef struct
{
    packet          base;
    int32_t         method;
    int32_t         keepalive;
    size_t          url;
    size_t          status;
    size_t          body;
    size_t          bodysize;
    list            headers;
}httppacket;

httppacket*
httppacket_new(bytebuffer *b);

int32_t
httppacket_on_header_field(httppacket *p,char *at, size_t length);

int32_t
httppacket_on_header_value(httppacket *p,char *at, size_t length);

void
httppacket_on_buffer_expand(httppacket *p,bytebuffer *b);

const char*
httppacket_get_header(httppacket *p,const char *field);

#endif