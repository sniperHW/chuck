#include "util/bit.h"

int main(){
	int a = 2;
	int b = 4;
	int c = 128;
	int d = 512;
	int e = 9288;
	int f = 132334143241;
	bitset_show(&a,sizeof(a)*8);
	bitset_show(&b,sizeof(b)*8);
	bitset_show(&c,sizeof(c)*8);
	bitset_show(&d,sizeof(d)*8);
	bitset_show(&e,sizeof(e)*8);
	bitset_show(&f,sizeof(f)*8);

	bitset *bb = bitset_new(32);

	bitset_set(bb,1);

	bitset_set(bb,3);

	bitset_set(bb,4);

	bitset_clear(bb,3);

	bitset_show(bb->bits,32);

	return 0;
};