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

//double link list
#ifndef _DLIST_H
#define _DLIST_H


struct dlist;

typedef struct dlistnode
{
    struct dlistnode *pre;
    struct dlistnode *next;
    struct dlist     *owner;
}dlistnode;

typedef struct dlist
{
    struct dlistnode head;
    struct dlistnode tail;
}dlist;

static inline int32_t 
dlist_empty(dlist *dl)
{
    return dl->head.next == &dl->tail ? 1:0;
}

static inline dlistnode*
dlist_begin(dlist *dl)
{
    return dl->head.next;
}

static inline dlistnode*
dlist_end(dlist *dl)
{
    return &dl->tail;
}

static inline dlistnode*
dlist_rbegin(dlist *dl)
{
    return dl->tail.pre;
}

static inline dlistnode*
dlist_rend(dlist *dl)
{
    return &dl->head;
}



static inline int32_t 
dlist_remove(dlistnode *dln)
{
    if(!dln->owner || (!dln->pre && !dln->next)) 
        return -1;
    dln->pre->next = dln->next;
    dln->next->pre = dln->pre;
    dln->pre = dln->next = NULL;
    dln->owner = NULL;
    return 0;
}

static inline dlistnode*
dlist_pop(dlist *dl)
{
    dlistnode *n = NULL;
    if(!dlist_empty(dl)){
        n = dl->head.next;
        dlist_remove(n);
    }
    return n;
}

static inline int32_t 
dlist_pushback(dlist *dl,dlistnode *dln)
{
    if(dln->owner || dln->pre || dln->next) 
        return -1;
    dl->tail.pre->next = dln;
    dln->pre = dl->tail.pre;
    dl->tail.pre = dln;
    dln->next = &dl->tail;
    dln->owner = dl; 
    return 0;
}

static inline int32_t 
dlist_pushfront(dlist *dl,dlistnode *dln)
{
    if(dln->owner || dln->pre || dln->next) 
        return -1;
    dlistnode *next = dl->head.next;
    dl->head.next = dln;
    dln->pre = &dl->head;
    dln->next = next;
    next->pre = dln;
    dln->owner = dl;
    return 0;
}

static inline void 
dlist_init(dlist *dl)
{
    dl->head.pre = dl->tail.next = NULL;
    dl->head.next = &dl->tail;
    dl->tail.pre = &dl->head;
}

//if the dblnk_check return != 0,dln will be remove
typedef int32_t (*dblnk_check)(dlistnode*,void *);

static inline void 
dlist_check_remove(dlist *dl,
                   dblnk_check _check,
                   void *ud)
{
    if(dlist_empty(dl)) return;

    dlistnode* dln = dlist_begin(dl);
    dlistnode* end = dlist_end(dl);
    while(dln != end)
    {
        dlistnode *tmp = dln;
        dln = dln->next;
        if(_check(tmp,ud)){
            //remove
            dlist_remove(tmp);
        }
    }
}

#endif
