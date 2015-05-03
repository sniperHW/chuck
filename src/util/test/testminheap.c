#include <stdio.h>
#include "util/minheap.h"

struct n{
	minheap_element base;
	int32_t num;
};


static int32_t less(minheap_element*l,minheap_element*r){
	return ((struct n*)l)->num < ((struct n*)r)->num ? 1:0;
}

#define INSERT(H,N)\
    {\
     struct n *node = calloc(1,sizeof(*node));\
     node->num = N;\
     minheap_insert(H,(minheap_element*)node);}

#define POPMIN(H)\
     ({  int32_t __result = -1;\
       struct n *node;\
       do{\
       	node = ( struct n*)minheap_popmin(H);\
       	if(node){\
       	 __result = node->num;\
       	 free(node);}\
       }while(0);\
       __result;})     


int main(){

	minheap *m = minheap_new(10,less);
	int i = 0;
	for(; i < 20; ++i){
		INSERT(m,i);
	}

	while((i = POPMIN(m)) != -1){
		printf("%u\n",i);
	}
	printf("-----------------------\n");
	i = 0;
	for(; i < 20; ++i){
		INSERT(m,20-i);
	}

	while((i = POPMIN(m)) != -1){
		printf("%u\n",i);
	}

	struct n *array[20];
	for(i = 0;i < 20; ++i){
		array[i] = calloc(1,sizeof(*array[i]));
		array[i]->num = i + 1;
		minheap_insert(m,(minheap_element*)array[i]);
	}

	array[0]->num = 100;
	array[3]->num = 101;
	array[7]->num = 88;

	minheap_change(m,(minheap_element*)array[0]);
	minheap_change(m,(minheap_element*)array[3]);
	minheap_change(m,(minheap_element*)array[7]);

	printf("-----------------------\n");

	while((i = POPMIN(m)) != -1){
		printf("%u\n",i);
	}


	return 0;
}
