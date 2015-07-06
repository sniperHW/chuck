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

#include "comm.h"
#include "util/bytebuffer.h"
#include "util/refobj.h"

enum{
	WPACKET = 1,
	RPACKET,
	RAWPACKET,
    HTTPPACKET,
	PACKET_END,
};

#ifndef TYPE_HEAD
#define TYPE_HEAD uint16_t
#endif

#define SIZE_HEAD sizeof(TYPE_HEAD)

#if TYPE_HEAD == uint16_t 

#define ntoh  _ntoh16
#define hton  _hton16

#elif TYPE_HEAD == uint32_t

#define ntoh  _ntoh32
#define hton  _hton32

#else
    #error "un support TYPE_HEAD!"
#endif  

typedef struct packet
{
    listnode    node;   
    bytebuffer* head;                                        //head or buff list
    struct packet*  (*construct_write)(struct packet*);
    struct packet*  (*construct_read)(struct packet*);
    struct packet*  (*clone)(struct packet*);
    void            (*dctor)(void*);
    uint64_t    sendtime;    
    uint32_t    len_packet;                                  //total size of packet in bytes        
    TYPE_HEAD   spos;                                        //start pos in head 
    uint8_t     type;
      
}packet;


#ifndef MIN_BUFFER_SIZE
#define MIN_BUFFER_SIZE 64
#endif

#define make_writepacket(p) cast(packet*,(p))->construct_write(cast(packet*,(p)))
#define make_readpacket(p)  cast(packet*,(p))->construct_read(cast(packet*,(p)))
#define clone_packet(p)     cast(packet*,(p))->clone(cast(packet*,(p))) 

void packet_del(packet*);


#endif