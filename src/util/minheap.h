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

#ifndef _MINHEAP_H
#define _MINHEAP_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct 
{
	int32_t i;//index in minheap;
}minheap_element;

typedef struct
{
	int32_t size;
	int32_t max_size;
	int32_t (*less)(minheap_element*l,minheap_element*r);//if l < r return 1,else return 0	
	minheap_element **data;
}minheap;


minheap*
minheap_new(int32_t size,
			int32_t (*)(minheap_element *l,minheap_element*r));

void     
minheap_del(minheap*);

//use to clear all heap elements
void     
minheap_clear(minheap*,void (*)(minheap_element*));

static inline void 
element_swap(minheap *m,int32_t i1,int32_t i2)
{
	minheap_element *ele = m->data[i1];
	m->data[i1] = m->data[i2];
	m->data[i2] = ele;
	m->data[i1]->i = i1;
	m->data[i2]->i = i2;
}

static inline int32_t 
element_parent(int32_t i)
{
	return i/2;
}

static inline int32_t 
element_left(int32_t i)
{
	return i*2;
}

static inline int32_t 
element_right(int32_t i)
{
	return i*2+1;
}

static inline void 
element_up(minheap *m,int32_t i)
{
	int32_t p = element_parent(i);
	while(p)
	{
		assert(m->data[i]);
		assert(m->data[p]);
		if(m->less(m->data[i],m->data[p]))
		{
			element_swap(m,i,p);
			i = p;
			p = element_parent(i);
		}
		else
			break;
	}
}

static inline void 
element_down(minheap *m,int32_t i)
{

	int32_t l = element_left(i);
	int32_t r = element_right(i);
	int32_t min = i;
	if(l <= m->size)
	{
		assert(m->data[l]);
		assert(m->data[i]);
	}

	if(l <= m->size && m->less(m->data[l],m->data[i]))
		min = l;

	if(r <= m->size)
	{
		assert(m->data[r]);
		assert(m->data[min]);
	}

	if(r <= m->size && m->less(m->data[r],m->data[min]))
		min = r;

	if(min != i)
	{
		element_swap(m,i,min);
		element_down(m,min);
	}		
}

static inline void 
minheap_change(minheap *m,minheap_element *e)
{
	int i = e->i;
	element_down(m,i);
	if(i == e->i)
		element_up(m,i);
}

static inline void 
minheap_insert(minheap *m,minheap_element *e)
{
	if(e->i)
		return minheap_change(m,e);
	if(m->size >= m->max_size-1)
	{
		//expand the heap
		uint32_t new_size = m->max_size*2;
		minheap_element **tmp = (minheap_element**)calloc(new_size,sizeof(minheap_element*));
		if(!tmp)
			return;
		memcpy(tmp,m->data,m->max_size*sizeof(minheap_element*));
		free(m->data);
		m->data = tmp;
		m->max_size = new_size;
	}	
	++m->size;
	m->data[m->size] = e;
	e->i = m->size;
	element_up(m,e->i);	
}

static inline void 
minheap_remove(minheap *m,minheap_element *e)
{
	int32_t i = e->i;
	if(i == 0) return;
	if(m->size > 1){
		minheap_element *other = m->data[m->size];
		element_swap(m,i,m->size);
		--m->size;
		element_down(m,other->i);
	}else{
		--m->size;
	}
	e->i = 0;
}


//return the min element
static inline minheap_element* 
minheap_min(minheap *m)
{
	if(!m->size)
		return NULL;
	return m->data[1];
}

//return the min element and remove it
static inline minheap_element* 
minheap_popmin(minheap *m)
{
	if(m->size)
	{
		minheap_element *e = m->data[1];
		element_swap(m,1,m->size);
		m->data[m->size] = NULL;
		--m->size;
		element_down(m,1);
		e->i = 0;
		return e;
	}
	return NULL;
}
#endif
