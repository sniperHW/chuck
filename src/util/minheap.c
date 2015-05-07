#include "minheap.h"
#include "comm.h"

minheap*
minheap_new(int32_t size,
			int32_t (*less)(minheap_element*,minheap_element*))
{
	size = size_of_pow2(size);
	minheap *m = calloc(1,sizeof(*m));
	m->data = (minheap_element**)calloc(size,sizeof(minheap_element*));
	m->size = 0;
	m->max_size = size;
	m->less = less;
	return m;
}

void 
minheap_del(minheap *m)
{
	free(m);
}

void 
minheap_clear(minheap *m,void (*f)(minheap_element*))
{
	uint32_t i;
	for(i = 1; i <= m->size; ++i){
		if(f)
			f(m->data[i]);
		m->data[i]->i = 0;
	}
	m->size = 0;
}
