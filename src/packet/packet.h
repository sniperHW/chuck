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

#ifndef _PACKET_H
#define _PACKET_H

#include "util/bytebuffer.h"
#include "util/refobj.h"

enum{
	WPACKET = 1,
	RPACKET,
	RAWPACKET,
	PACKET_END,
};

typedef struct packet
{
    listnode    node;   
    bytebuffer* head;                                        //head or buff list
    struct packet*  (*construct_write)(struct packet*);
    struct packet*  (*construct_read)(struct packet*);
    struct packet*  (*clone)(struct packet*);
    uint32_t    len_packet;                                  //total size of packet in bytes        
    uint32_t    spos:16;                                     //start pos in head 
    uint32_t    type:8;
    uint32_t    mask:8;      
}packet;

#define TYPE_HEAD uint16_t

#define SIZE_HEAD sizeof(TYPE_HEAD)

#define MIN_BUFFER_SIZE 64


#define make_writepacket(p) ((packet*)(p))->construct_write((packet*)(p))
#define make_readpacket(p)  ((packet*)(p))->construct_read((packet*)(p))
#define clone_packet(p)     ((packet*)(p))->clone((packet*)(p))

void packet_del(packet*);


#endif