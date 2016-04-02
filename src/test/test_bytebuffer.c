#include <stdio.h>
#include "util/chk_bytechunk.h"


int main()
{
	chk_bytebuffer *b = chk_bytebuffer_new(NULL,0,64);
	chk_bytebuffer_del(b);
	return 0;
}