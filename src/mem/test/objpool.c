#include <stdio.h>
#include "mem/obj_allocator.h"
#include "util/time.h"

int main(){
	allocator *objpool = objpool_new(sizeof(uint32_t),4096);
	

	uint32_t tick = systick32();

	uint32_t *array[1000000];
	int j = 0;
	for(; j < 100; ++j){
		int i =0;
		for(; i < 1000000; ++i){
			array[i] = ALLOC(objpool,1);
		}
		for(i = 0; i < 1000000; ++i){
			FREE(objpool,array[i]);
		}
	}

	printf("%f per sec\n",(double)(1000000*100)/((double)((systick32() - tick))/1000));

	return 0;
}