#include "util/chk_string.h"

int main() {
	chk_string *string = chk_string_new("hello");
	printf("%s,%d\n",chk_string_c_str(string),chk_string_size(string));
	chk_string_append(string,"hello");
	printf("%s,%d\n",chk_string_c_str(string),chk_string_size(string));
	chk_string_append(string,"hello");
	printf("%s,%d\n",chk_string_c_str(string),chk_string_size(string));
	chk_string_append(string,"hello");
	printf("%s,%d\n",chk_string_c_str(string),chk_string_size(string));	
	chk_string_append(string,"hello");
	printf("%s,%d\n",chk_string_c_str(string),chk_string_size(string));
	chk_string_destroy(string);					
	return 0;
}