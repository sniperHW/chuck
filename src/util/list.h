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

//link list

#ifndef _LIST_H
#define _LIST_H

#include <stdint.h>
#include <stdlib.h>


typedef struct listnode
{
    struct listnode *next;
}listnode;


typedef struct
{
	int32_t        size;
    listnode      *head;
    listnode      *tail;
}list;

static inline void list_init(list *l){
	l->head = l->tail = NULL;
	l->size = 0;
}

static inline void list_pushback(list *l,listnode *n)
{
    if(n->next) 
    	return;
	if(0 == l->size)
		l->head = l->tail = n;
	else{
		l->tail->next = n;
		l->tail = n;
	}
	++l->size;
}

static inline void list_pushfront(list *l,listnode *n)
{
    if(n->next) 
    	return;
	if(0 == l->size)
		l->head = l->tail = n;
	else{
		n->next = l->head;
		l->head = n;
	}
	++l->size;
}

static inline listnode* list_pop(list *l)
{
	listnode *head = l->head;
	if(!head) return NULL;
	l->head = head->next;
	if(!l->head) l->tail = NULL;
	--l->size;
	head->next = NULL;
	
	return head;
}

static inline int32_t list_size(list *l)
{
	return l->size;
}

static inline listnode* list_begin(list *l)
{
	return l->head;
}

static inline listnode* list_end(list *l)
{
	return NULL;
}


//append all the elements of b to a
static inline void list_pushlist(list *a,list *b)
{
	if(a == b) 
		return;	
	
	if(b->head && b->tail)
	{
		if(a->tail)
			a->tail->next = b->head;
		else
			a->head = b->head;
		a->tail = b->tail;
		b->head = b->tail = NULL;
		a->size += b->size;
		b->size = 0;
	}
}

#endif
