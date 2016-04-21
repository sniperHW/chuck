#ifndef _CHK_LIST_H
#define _CHK_LIST_H

#include <stdint.h>
#include <stdlib.h>
#include "util/chk_error.h"

typedef struct chk_list_entry chk_list_entry;

typedef struct chk_list chk_list;

typedef struct chk_dlist_entry chk_dlist_entry;

typedef struct chk_dlist chk_dlist;

struct chk_list_entry {
    chk_list_entry *next;
};

struct chk_list {
	size_t          size;
    chk_list_entry *head;
    chk_list_entry *tail;
};

struct chk_dlist_entry {
    chk_dlist_entry *pperv;
    chk_dlist_entry *next;
};

struct chk_dlist {
    chk_dlist_entry head;
    chk_dlist_entry tail;
};

static inline void chk_list_init(chk_list *l) {
	l->head = l->tail = NULL;
	l->size = 0;
}

//if push success,return the new size,else return -1
static inline size_t chk_list_pushback(chk_list *l,chk_list_entry *n) {
    if(n->next) return chk_error_common;
	if(0 == l->size) l->head = l->tail = n;
	else {
		l->tail->next = n;
		l->tail = n;
	}
	++l->size;
    return chk_error_ok;
}

static inline size_t chk_list_pushfront(chk_list *l,chk_list_entry *n) {
    if(n->next) return chk_error_common;
	if(0 == l->size) l->head = l->tail = n;
	else {
		n->next = l->head;
		l->head = n;
	}
	++l->size;
    return chk_error_ok;
}

static inline chk_list_entry *chk_list_pop(chk_list *l) {
	chk_list_entry *head = l->head;
	if(!head) return NULL;
	l->head = head->next;
	if(!l->head) l->tail = NULL;
	--l->size;
	head->next = NULL;
	return head;
}

static inline size_t chk_list_size(chk_list *l) {
	return l->size;
}

static inline int32_t chk_list_empty(chk_list *l) {
    return l->size == 0;
}

static inline chk_list_entry *chk_list_begin(chk_list *l) {
	return l->head;
}

static inline chk_list_entry *chk_list_end(chk_list *l) {
	return NULL;
}

//append all the elements of b to a
static inline void chk_list_pushlist(chk_list *a,chk_list *b) {
	if(a == b) return;	
	if(b->head && b->tail) {
		if(a->tail) a->tail->next = b->head;
		else a->head = b->head;
		a->tail = b->tail;
		b->head = b->tail = NULL;
		a->size += b->size;
		b->size = 0;
	}
}

#define chk_list_foreach(LIST,IT)\
 for((IT)=chk_list_begin((LIST)); (IT) != chk_list_end((LIST)); (IT) = (IT)->next)

/////chk_dlist

static inline void chk_dlist_init(chk_dlist *dl) {
    dl->head.pperv = dl->tail.next = NULL;
    dl->head.next  = &dl->tail;
    dl->tail.pperv = &dl->head;
}


static inline int32_t chk_dlist_empty(chk_dlist *dl) {
    return dl->head.next == &dl->tail ? 1:0;
}

static inline chk_dlist_entry *chk_dlist_begin(chk_dlist *dl) {
    return dl->head.next;
}

static inline chk_dlist_entry *chk_dlist_end(chk_dlist *dl) {
    return &dl->tail;
}


static inline int32_t chk_dlist_remove(chk_dlist_entry *dln) {
    if(!dln->pperv || !dln->next) return chk_error_common;
    dln->pperv->next = dln->next;
    dln->next->pperv = dln->pperv;
    dln->pperv       = dln->next = NULL;
    return 0;
}

static inline chk_dlist_entry *chk_dlist_pop(chk_dlist *dl) {
    chk_dlist_entry *n = NULL;
    if(!chk_dlist_empty(dl)) {
        n = dl->head.next;
        chk_dlist_remove(n);
    }
    return n;
}

static inline int32_t chk_dlist_pushback(chk_dlist *dl,chk_dlist_entry *dln) {
    if(dln->pperv || dln->next) return chk_error_common;
    dl->tail.pperv->next = dln;
    dln->pperv           = dl->tail.pperv;
    dl->tail.pperv       = dln;
    dln->next            = &dl->tail;
    return 0;
}

static inline int32_t chk_dlist_pushfront(chk_dlist *dl,chk_dlist_entry *dln) {
    if(dln->pperv || dln->next) return chk_error_common;
    chk_dlist_entry *next = dl->head.next;
    dl->head.next         = dln;
    dln->pperv            = &dl->head;
    dln->next             = next;
    next->pperv           = dln;
    return 0;
}

static inline void chk_dlist_move(chk_dlist *dest,chk_dlist *src) {
    dest->head.next = src->head.next;
    dest->head.next->pperv = &dest->head;
    dest->tail.pperv = src->tail.pperv;
    dest->tail.pperv->next = &dest->tail;
    chk_dlist_init(src);
}

#define chk_dlist_foreach(DLIST,IT)\
 for((IT)=chk_dlist_begin((DLIST)); (IT) != chk_dlist_end((DLIST)); (IT) = (IT)->next)


#endif