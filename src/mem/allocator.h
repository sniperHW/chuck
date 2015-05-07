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

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <stdlib.h>
#include <stdint.h>

typedef struct allocator{
	void* (*alloc)(struct allocator*,size_t);
	void* (*calloc)(struct allocator*,size_t,size_t);
	void* (*realloc)(struct allocator*,void*,size_t);
	void  (*free)(struct allocator*,void*);
}allocator;

#define ALLOC(A,SIZE)\
	({void* __result;\
	   if(!A) 	__result = malloc(SIZE);\
	   else  __result = ((allocator*)A)->alloc((allocator*)A,SIZE);\
	  __result;})

#define CALLOC(A,NUM,SIZE)\
	({void* __result;\
	   if(!A) 	__result = calloc(NUM,SIZE);\
	   else  __result = ((allocator*)A)->calloc((allocator*)A,NUM,SIZE);\
	  __result;})

#define REALLOC(A,PTR,SIZE) do{if(!A) realloc(PTR,SIZE);\
								else  ((allocator*)A)->realloc((allocator*)A,PTR,SIZE);\
							}while(0)

#define FREE(A,PTR) do{if(!A) free(PTR);\
					   else   ((allocator*)A)->free((allocator*)A,PTR);\
					}while(0)

#endif