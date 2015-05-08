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

#ifndef _REDIS_CLIENT_H
#define _REDIS_CLIENT_H

#include <stdint.h>
#include "socket/socket.h"  
#include "lua/lua_util.h"

#define REDIS_REPLY_STRING  1
#define REDIS_REPLY_ARRAY   2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_NIL     4
#define REDIS_REPLY_STATUS  5
#define REDIS_REPLY_ERROR   6


#define REDIS_OK     0
#define REDIS_RETRY -1
#define REDIS_ERR   -2    

typedef struct redisReply {
    int32_t type; /* REDIS_REPLY_* */
    int64_t integer; /* The integer when type is REDIS_REPLY_INTEGER */
    int32_t len; /* Length of string */
    char   *str; /* Used for both REDIS_REPLY_ERROR and REDIS_REPLY_STRING */
    size_t  elements; /* number of elements, for REDIS_REPLY_ARRAY */
    struct redisReply **element; /* elements vector for REDIS_REPLY_ARRAY */
}redisReply;

typedef struct redis_conn redis_conn;

redis_conn*
redis_connect(engine *e,sockaddr_ *addr,
              void (*)(redis_conn*,int32_t err));

int32_t
redis_asyn_connect(engine *e,sockaddr_ *addr,
                   void (*)(redis_conn*,int32_t err,void*),
                   void*);

void        
redis_close(redis_conn*);

int32_t     
redis_query(redis_conn*,const char *str,
            void (*)(redis_conn*,redisReply*,void *ud),
            void *ud);


void 
redis_set_clearcb(redis_conn*,void (*)(void*ud));


void
reg_luaredis(lua_State *L);


//for test
void 
test_parse_reply(char *str);




#endif    
