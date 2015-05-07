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


#define REDIS_STATUS  '+'    //状态回复（status reply）
#define REDIS_ERROR   '-'    //错误回复（error reply）
#define REDIS_IREPLY  ':'    //整数回复（integer reply）
#define REDIS_BREPLY  '$'    //批量回复（bulk reply）
#define REDIS_MBREPLY '*'    //多条批量回复（multi bulk reply）

typedef struct redisReply {
    int32_t type; /* REDIS_REPLY_* */
    int64_t integer; /* The integer when type is REDIS_REPLY_INTEGER */
    int32_t len; /* Length of string */
    char   *str; /* Used for both REDIS_REPLY_ERROR and REDIS_REPLY_STRING */
    size_t  elements; /* number of elements, for REDIS_REPLY_ARRAY */
    struct redisReply **element; /* elements vector for REDIS_REPLY_ARRAY */
}redisReply;

typedef struct redis_conn redis_conn;

redis_conn *redis_connect(engine *e,sockaddr_ *addr,void (*on_disconnect)(redis_conn*,int32_t err));
void        redis_close(redis_conn*);
int32_t     redis_query(redis_conn*,const char *str,void (*)(redisReply*,void *ud),void *ud);


#endif    
