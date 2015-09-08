#ifndef _HTTPPACKET_H
#define _HTTPPACKET_H
#include <stdarg.h>
#include "comm.h"    
#include "packet/packet.h"
#include "util/endian.h"
#include "util/string.h"

typedef struct st_header{
	listnode node;
	string  *field;
	string  *value;
}st_header;

typedef struct
{
    packet          base;
    string         *url;
    string         *status;
    string         *body;
    list            headers;
    int32_t         method;
    st_header      *current;
}httppacket;

httppacket *httppacket_new();

int32_t     httppacket_on_header_field(httppacket *p,char *at, size_t length);

int32_t     httppacket_on_header_value(httppacket *p,char *at, size_t length);

//int32_t     httppacket_on_body(httppacket *p,char *at, size_t length);

//string     *httppacket_get_header(httppacket *p,const char *field);

#endif