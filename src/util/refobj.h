/*
    Copyright (C) <2014>  <sniperHW@163.com>

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
#ifndef _REFOBJ_H
#define _REFOBJ_H

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <assert.h>
#include "util/atomic.h"
#include "util/exception.h"  
  
typedef struct
{
        volatile uint32_t refcount;
        uint32_t          pad1;
        volatile uint32_t flag;  
        uint32_t          pad2;      
        union{
            struct{
                volatile uint32_t low32; 
                volatile uint32_t high32;       
            };
            volatile uint64_t identity;
        };
        void (*dctor)(void*);
}refobj;

void 
refobj_init(refobj *r,void (*dctor)(void*));

static inline uint32_t 
refobj_inc(refobj *r)
{
    return ATOMIC_INCREASE_FETCH(&r->refcount);
}

uint32_t 
refobj_dec(refobj *r);

typedef struct{
    union{  
        struct{ 
            uint64_t identity;    
            refobj   *ptr;
        };
        uint32_t _data[4];
    };
}refhandle;


refobj*
cast2refobj(refhandle h);

static inline void 
refhandle_clear(refhandle *h)
{
    h->ptr = NULL;
    h->identity = 0;
}

static inline int32_t 
is_vaild_refhandle(refhandle *h){
    if(!h->ptr || !(h->identity > 0))
        return 0;
    return 1;
}

static inline refhandle 
get_refhandle(refobj *o)
{
    return (refhandle){.identity=o->identity,.ptr=o}; 
} 

#endif
