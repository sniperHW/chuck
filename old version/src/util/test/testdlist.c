#include <stdio.h>
#include <stdlib.h>
#include "util/dlist.h"


typedef struct{
	dlistnode base;
	int n;
}nnode;


#define PUSH_BACK(DLIST,VAL)\
    {\
     nnode *node = calloc(1,sizeof(*node));\
     node->n = VAL;\
     dlist_pushback(DLIST,(dlistnode*)node);}


#define PUSH_FRONT(DLIST,VAL)\
    {\
     nnode *node = calloc(1,sizeof(*node));\
     node->n = VAL;\
     dlist_pushfront(DLIST,(listnode*)node);}


#define POP(DLIST)\
   ({  int __result = 0;\
       nnode *node;\
       do{\
       	node = (nnode*)dlist_pop(DLIST);\
       	if(node){\
       	 __result = node->n;\
       	 free(node);}\
       }while(0);\
       __result;})


int32_t _dblnk_check(dlistnode *_1,void *_2){
	if(((nnode*)_1)->n % 2 == 0)
		return 1;
	return 0;
}

int main(){
	dlist l;
	dlist_init(&l);
	int i = 0;
	for(;i < 10;++i){
		PUSH_BACK(&l,i);
	}

	for(i-=1;i >= 0;i--){
		printf("%d\n",POP(&l));
	}

	printf("-----------reverse---------\n");

	for(i = 0;i < 10;++i){
		PUSH_BACK(&l,i);
	}

	for(i-=1;i >= 0;i--){
		printf("%d\n",POP(&l));
	}

	printf("-----------check_remove---------\n");

	for(i = 0;i < 10;++i){
		PUSH_BACK(&l,i);
	}

	dlist_check_remove(&l,_dblnk_check,NULL);

	dlistnode *n = dlist_begin(&l);
	dlistnode *end = dlist_end(&l);
	for(; n != end; n = n->next){
		printf("%d\n",((nnode*)n)->n);
	}

	printf("-----------reverse---------\n");

	n = dlist_rbegin(&l);
	end = dlist_rend(&l);
	for(; n != end; n = n->pre){
		printf("%d\n",((nnode*)n)->n);
	}	

	return 0;
}