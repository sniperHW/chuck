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

#ifndef _RAWPACKET_H
#define _RAWPACKET_H

#include "packet/packet.h"
#include "mem/allocator.h"
#include "util/endian.h"

extern allocator* g_rawpk_allocator;

typedef struct
{
    packet          base;
    buffer_writer   writer;
}rawpacket;

rawpacket *rawpacket_new(uint32_t size);

//will add reference count of b
rawpacket *rawpacket_new_by_buffer(bytebuffer *b,uint32_t spos);

static inline void rawpacket_expand(rawpacket *raw,uint32_t newsize)
{

	newsize = size_of_pow2(newsize);
    if(newsize < MIN_BUFFER_SIZE) newsize = MIN_BUFFER_SIZE;
    bytebuffer *newbuff = bytebuffer_new(newsize);
   	bytebuffer *oldbuff = ((packet*)raw)->head;
   	memcpy(newbuff->data,oldbuff->data,oldbuff->size);
   	newbuff->size = oldbuff->size;
   	refobj_dec((refobj*)oldbuff);
    ((packet*)raw)->head = newbuff;
    //set writer to the end
    buffer_writer_init(&raw->writer,newbuff,((packet*)raw)->len_packet);
}

static inline void rawpacket_copy_on_write(rawpacket *raw)
{
	rawpacket_expand(raw,((packet*)raw)->len_packet);
}

static inline void rawpacket_append(rawpacket *raw,void *_,uint32_t size)
{
	char *in = (char*)_;
    if(!raw->writer.cur)
        rawpacket_copy_on_write(raw);
    else{
		uint32_t packet_len = ((packet*)raw)->len_packet;
    	uint32_t new_size = packet_len + size;
    	assert(new_size >= packet_len);
    	if(new_size <= packet_len) return;
    	if(new_size > ((packet*)raw)->head->cap){
    		rawpacket_expand(raw,new_size);
    	}
    	buffer_write(&raw->writer,in,size);
    	((packet*)raw)->len_packet = new_size;
    }	
}

static inline void *rawpacket_data(rawpacket *raw,uint32_t *size){
	if(size) *size = ((packet*)raw)->len_packet;
	return (void*)(((packet*)raw)->head->data + ((packet*)raw)->spos);
}

#endif