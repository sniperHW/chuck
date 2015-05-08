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

#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include "socket/wrap/decoder.h"
#include "socket/socket.h"  
#include "socket/wrap/wrap_comm.h"
#include "lua/lua_util.h" 

enum{
    PKEV_RECV,            //recv a packet
    PKEV_SEND,            //send a packet finish
};

typedef struct connection{
    stream_socket_ base;
    struct         iovec wsendbuf[MAX_WBAF];
    struct         iovec wrecvbuf[2];
    iorequest      send_overlap;
    iorequest      recv_overlap;
    uint32_t       next_recv_pos;
    bytebuffer    *next_recv_buf;        
    list           send_list;//待发送的包
    uint32_t       recv_bufsize;
    void           (*on_packet)(struct connection*,packet*,int32_t event);
    void           (*on_disconnected)(struct connection*,int32_t err);
    decoder       *decoder_;
    luaRef         lua_cb_packet;
    luaRef         lua_cb_disconnected;
}connection;

connection*
connection_new(int32_t fd,uint32_t buffersize,
               decoder *d);

int32_t     
connection_send(connection *c,packet *p,
                int32_t send_fsh_notify);

void        
connection_close(connection *c);

decoder*
conn_raw_decoder_new();


static inline void 
connection_set_discntcb(connection *c,
                        void(*on_disconnected)
                        (connection*,int32_t))
{
    c->on_disconnected = on_disconnected;
}

void        
reg_luaconnection(lua_State *L);

#endif    
