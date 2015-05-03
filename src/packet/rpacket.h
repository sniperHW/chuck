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

#ifndef _RPACKET_H
#define _RPACKET_H

#include "packet/packet.h"
#include "mem/allocator.h"
#include "util/endian.h"    

extern allocator* g_rpk_allocator;

typedef struct{
    packet          base;
    buffer_reader   reader;
    uint16_t        data_remain; 
    //for corssing buffer binary data read
    bytebuffer     *binbuf;
    uint16_t        binpos;
}rpacket;

//will add reference count of b
rpacket *rpacket_new(bytebuffer *b,uint32_t start_pos);

static inline uint16_t rpacket_read(rpacket *r,char *out,uint16_t size){
    if(size > r->data_remain) return 0;
    uint16_t out_size = buffer_read(&r->reader,out,(uint16_t)size);
    assert(out_size == size);
    r->data_remain -= out_size;
    return out_size;
}

static inline uint16_t rpacket_peek(rpacket *r,char *out,uint16_t size){
    if(size > r->data_remain) return 0;
    bytebuffer *back1 = r->reader.cur;
    uint32_t    back2 = r->reader.pos;
    uint16_t out_size = (uint16_t)buffer_read(&r->reader,out,(uint16_t)size);
    assert(out_size == size);
    //recover
    buffer_reader_init(&r->reader,back1,back2);
    return out_size;
}


const char *rpacket_read_string(rpacket*);
const void *rpacket_read_binary(rpacket*,uint16_t *len);

#define RPACKET_READ(R,TYPE)\
        ({TYPE __result;rpacket_read(R,(char*)&__result,sizeof(TYPE));__result;})

#define RPACKET_PEEK(R,TYPE)\
        ({TYPE __result;rpacket_read(R,(char*)&__result,sizeof(TYPE));__result;})

static inline uint8_t rpacket_read_uint8(rpacket *r){
    return RPACKET_READ(r,uint8_t);
}

static inline uint16_t rpacket_read_uint16(rpacket *r){
    return _ntoh16(RPACKET_READ(r,uint16_t));
}

static inline uint32_t rpacket_read_uint32(rpacket *r){
    return _ntoh32(RPACKET_READ(r,uint32_t));
}

static inline uint64_t rpacket_read_uint64(rpacket *r){
    return _ntoh64(RPACKET_READ(r,uint64_t));
}

static inline double rpacket_read_double(rpacket *r){
    return RPACKET_READ(r,double);
}

static inline uint8_t rpacket_peek_uint8(rpacket *r){
    return RPACKET_PEEK(r,uint8_t);
}

static inline uint16_t rpacket_peek_uint16(rpacket *r){
    return _ntoh16(RPACKET_PEEK(r,uint16_t));
}

static inline uint32_t rpacket_peek_uint32(rpacket *r){
    return _ntoh32(RPACKET_PEEK(r,uint32_t));
}

static inline uint64_t rpacket_peek_uint64(rpacket *r){
    return _ntoh64(RPACKET_PEEK(r,uint64_t));
}

static inline double rpacket_peek_double(rpacket *r){
    return RPACKET_PEEK(r,double);
}

#endif    