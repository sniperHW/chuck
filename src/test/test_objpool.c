#include <stdio.h>
#include "util/chk_obj_pool.h"

DECLARE_OBJPOOL(int)

int main(){
	int_chk_objpool *pool = int_chk_objpool_new(1024);

	printf("chunk:%u,free:%u,used:%u\n",pool->chunkcount,pool->freecount,pool->usedcount);

	int *array[4096];
	int i;
	for(i = 0; i < 4096; ++i) {
		array[i] = int_new(pool);
	}

	printf("chunk:%u,free:%u,used:%u\n",pool->chunkcount,pool->freecount,pool->usedcount);

	for(i = 0; i < 4096; ++i) {
		int_release(pool,array[i]);
	}

	printf("chunk:%u,free:%u,used:%u\n",pool->chunkcount,pool->freecount,pool->usedcount);
	return 0;
}
