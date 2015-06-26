#include "util/string.h"
#include <stdio.h>

int main(){
	string *s = string_new("");
	string_append(s,"a","b","c");
	printf("%s\n",string_cstr(s));	
	return 0;
}