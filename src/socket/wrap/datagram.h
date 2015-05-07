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

#ifndef _DATAGRAM_H_
#define _DATAGRAM_H_

#include "socket/wrap/decoder.h"
#include "socket/socket.h"  
#include "socket/wrap/wrap_comm.h"
#include "lua/lua_util.h"      

typedef struct datagram{
    dgram_socket_ base;
    struct        iovec wsendbuf[MAX_WBAF];
    struct        iovec wrecvbuf[2];
    iorequest     recv_overlap;
    uint32_t      next_recv_pos;
    bytebuffer   *next_recv_buf;        
    uint32_t      recv_bufsize;
    void          (*on_packet)(struct datagram*,packet*,sockaddr_*);
    decoder      *decoder_;
}datagram;


datagram *datagram_new(int32_t fd,uint32_t buffersize,decoder *d);
int32_t   datagram_send(datagram *d,packet *p,sockaddr_ *addr);
void      datagram_close(datagram *d);

decoder  *dgram_raw_decoder_new();

int32_t   datagram_reg_tolua(lua_State *L);


#endif    