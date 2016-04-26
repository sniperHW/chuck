#ifndef _CHK_BITSET_H
#define _CHK_BITSET_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct chk_bitset chk_bitset;

struct chk_bitset {
	uint32_t size;
	uint32_t bits[];
};

static inline chk_bitset *chk_bitset_new(uint32_t size) {
	uint32_t _size = size % sizeof(uint32_t) == 0 ? size/sizeof(uint32_t):size/sizeof(uint32_t)+1;
	chk_bitset *bs = calloc(1,sizeof(*bs)+sizeof(uint32_t)*_size);
	if(!bs) return NULL;
	bs->size = size;
	return bs;
}

static inline void chk_bitset_del(chk_bitset *bs) {
	free(bs);
}

static inline void chk_bitset_set(chk_bitset *bs,uint32_t index) {
	uint32_t b_index;
	if(index <= bs->size) {
		b_index = index / (sizeof(uint32_t)*8);
		index %= (sizeof(uint32_t)*8);
		bs->bits[b_index] = bs->bits[b_index] | (1 << index);
	}
}

static inline void chk_bitset_clear(chk_bitset *bs,uint32_t index) {
	uint32_t b_index;
	if(index <= bs->size) {
		b_index = index / (sizeof(uint32_t)*8);
		index %= (sizeof(uint32_t)*8);
		bs->bits[b_index] = bs->bits[b_index] ^ (1<<index);
	}
}

static inline int32_t chk_bitset_test(chk_bitset *bs,uint32_t index) {
	uint32_t b_index;
	if(index <= bs->size) {
		b_index = index / (sizeof(uint32_t)*8);
		index %= (sizeof(uint32_t)*8);
		return bs->bits[b_index] & (1 << index)?1:0;
	}else
		return 0;
}

//just for test
static inline void chk_bitset_show(void *ptr,uint32_t size)
{
	int i;
	chk_bitset *b = chk_bitset_new(size);
	memcpy(b->bits,ptr,size);
	for(i = b->size-1; i >=0; --i){
		printf("%d",chk_bitset_test(b,i)?1:0);
	} 
	printf("\n");
	chk_bitset_del(b);	
}
	
#endif