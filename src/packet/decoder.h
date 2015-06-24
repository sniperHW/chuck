/*
    Copyright (C) <2015>  <sniperHW@163.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _DECODER_H
#define _DECODER_H

#include "comm.h"
#include "packet/packet.h"
#include "packet/httppacket.h"    
#include "util/bytebuffer.h"
#include "lua/lua_util.h"
#include "http-parser/http_parser.h"

/*enum {
    DERR_TOOLARGE  = -1,
    DERR_UNKNOW    = -2,
};*/

typedef struct decoder decoder;    

#define decoder_head                                                \
    packet     *(*unpack)(struct decoder*,int32_t *err);            \
    void        (*dctor)(struct decoder*);                          \
    bytebuffer *buff;                                               \
    uint32_t    pos;                                                \
    uint32_t    size;                                               \
    uint32_t    max_packet_size;                                    \
    void (*decoder_update)(decoder*,bytebuffer*,uint32_t,uint32_t)
//解包器接口
typedef struct decoder{
    decoder_head;
}decoder;

typedef struct httpdecoder{
    decoder_head;
    http_parser          parser;
    http_parser_settings settings;
    httppacket          *packet;
    int                  status;   
}httpdecoder;


decoder*
rpacket_decoder_new(uint32_t max_packet_size);

decoder*
conn_raw_decoder_new();

decoder*
dgram_raw_decoder_new();

decoder*
http_decoder_new(uint32_t max_packet_size);

void 
decoder_del(decoder*);

#ifdef _CHUCKLUA

void 
reg_luadecoder(lua_State *L);

#endif

#endif    