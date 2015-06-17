#ifndef _HTTPPACKET_H
#define _HTTPPACKET_H
#include <stdarg.h>
#include "comm.h"    
#include "packet/packet.h"
#include "mem/allocator.h"
#include "util/endian.h"


typedef struct
{
    packet          base;
}httppacket;


httppacket*
httppacket_new();

int
httppacket_onurl(httppacket*,const char *at, size_t length);

int
httppacket_onstatus(httppacket*,const char *at, size_t length);

int
httppacket_on_header_field(httppacket*,const char *at, size_t length);	

int
httppacket_on_header_value(httppacket*,const char *at, size_t length);

int
httppacket_on_body(httppacket*,const char *at, size_t length);	

#endif