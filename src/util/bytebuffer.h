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

#ifndef _BYTEBUFFER_H
#define _BYTEBUFFER_H

#include "util/refobj.h"
#include "comm.h"

typedef struct bytebuffer{
	refobj   			base;
	uint32_t 			cap;     //capacity;
	uint32_t 		    size;    //data size
	struct  bytebuffer *next;    
	char                data[0];   
}bytebuffer;


static inline void 
bytebuffer_dctor(void *_)
{
	bytebuffer *b = cast(bytebuffer*,_);
	if(b->next)
		refobj_dec(cast(refobj*,b->next));
    free(b);
}


static inline bytebuffer*
bytebuffer_new(uint32_t capacity)
{
	uint32_t size = sizeof(bytebuffer) + capacity;
    bytebuffer *b = cast(bytebuffer*,malloc(size));
	if(b){   
		memset(b,0,sizeof(*b));
		b->cap = capacity;
		refobj_init(cast(refobj*,b),bytebuffer_dctor);
	}
	return b;
}

static inline void 
bytebuffer_set(bytebuffer **b1,bytebuffer *b2)
{
    if(*b1 == b2) return;
    if(b2)  refobj_inc(cast(refobj*,b2));
    if(*b1) refobj_dec(cast(refobj*,*b1));
	*b1 = b2;
}


typedef struct{
	bytebuffer *cur;
	uint32_t    pos;
}buffer_reader;

typedef struct{
	bytebuffer *cur;
	uint32_t    pos;	
}buffer_writer;


static inline void 
buffer_reader_init(buffer_reader *reader,
				   bytebuffer *buff,
				   uint32_t pos)
{
	reader->cur = buff;
	reader->pos = pos;
}

static inline void 
buffer_writer_init(buffer_writer *writer,
				   bytebuffer *buff,
				   uint32_t pos)
{
	writer->cur = buff;
	writer->pos = pos;
}

static inline int32_t 
reader_check_size(buffer_reader *reader,
				  uint32_t size)
{
	uint32_t tmp = reader->pos + size;
	return reader->cur->size >= tmp && tmp > size;
}

static inline uint32_t 
buffer_read(buffer_reader *reader,
			void *_,uint32_t size)
{
	uint32_t copy_size;
	uint32_t out_size = 0;
	char 	*out = cast(char*,_);
	char    *ptr;
	bytebuffer *b = reader->cur;
	while(b && size){
        copy_size = b->size - reader->pos;
		if(copy_size > 0){
			copy_size = copy_size > size ? size : copy_size;
			ptr = b->data + reader->pos;
			switch(copy_size){
				case 1:{
					*out = *ptr;
					break;
				}case 2:{
					*cast(uint16_t*,out) = *cast(uint16_t*,ptr);
					break;
				}case 4:{
					*cast(uint32_t*,out) = *cast(uint32_t*,ptr);
					break;
				}case 8:{
					*cast(uint64_t*,out) = *cast(uint64_t*,ptr);
					break;
				}default:{								
					memcpy(out,ptr,copy_size);
					break;
				}
			}
			size -= copy_size;
			reader->pos += copy_size;
			out += copy_size;
			out_size += copy_size;
		}
		if(b->next && reader->pos >= b->size){
			reader->pos = 0;
			b = reader->cur = b->next;
		}
	}
	return out_size;
}

static inline uint32_t 
buffer_write(buffer_writer *writer,
			 void *_,uint32_t size)
{
    uint32_t copy_size;
    uint32_t in_size = 0;
    char 	*in = cast(char*,_);
    char    *ptr;
    bytebuffer *b = writer->cur;
    while(b && size){
        copy_size = b->cap - writer->pos;
        if(copy_size > 0){
	        copy_size = copy_size > size ? size : copy_size;  
	        ptr = b->data + writer->pos;
			switch(copy_size){
				case 1:{
					*ptr = *in;
					break;
				}case 2:{
					*cast(uint16_t*,ptr) = *cast(uint16_t*,in);
					break;
				}case 4:{
					*cast(uint32_t*,ptr) = *cast(uint32_t*,in);
					break;
				}case 8:{
					*cast(uint64_t*,ptr) = *cast(uint64_t*,in);
					break;
				}default:{								
	        		memcpy(ptr,in,copy_size);
					break;
				}
			}
	        size -= copy_size;
	        b->size += copy_size;
	        writer->pos += copy_size;
	        in += copy_size;
	        in_size += copy_size;
    	}
        if(b->next && writer->pos >= b->cap){
            writer->pos = 0;
            b = writer->cur = b->next;
        }else
        	break;
    }
    return in_size;
}

#endif