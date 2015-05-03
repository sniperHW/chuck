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
#include "util/bytebuffer.h"

enum {
    DERR_TOOLARGE  = -1,
    DERR_UNKNOW    = -2,
};

//解包器接口
typedef struct decoder{
    packet     *(*unpack)(struct decoder*,int32_t *err);
    void        (*dctor)(struct decoder*);
    bytebuffer *buff;
    uint32_t    pos;
    uint32_t    size;//data size in byte waiting unpack     
    uint32_t    max_packet_size;
}decoder;


decoder *rpacket_decoder_new(uint32_t max_packet_size);

void decoder_del(decoder*);

static inline void decoder_init(decoder *d,bytebuffer *buff,uint32_t pos){
    d->buff = buff;
    refobj_inc((refobj*)buff);
    d->pos  = pos;
    d->size = 0;
}


#endif    