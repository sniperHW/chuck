#include "chk_string.h"
#include <string.h>
#include <stdlib.h>

chk_string *chk_string_new(const char *ptr) {
	chk_string *str = calloc(1,sizeof(*str));
	if(!str) {
		return NULL;
	}
	str->ptr = str->buff;
	if(ptr) {
		str->len = strlen(ptr);
		if(str->len >= CHK_STRING_DEFAULT_BUFF_SIZE) {
			str->ptr = calloc(1,str->len+1);
			if(!str->ptr) {
				free(str->ptr);
				str = NULL;
			}
		}
		strcpy(str->ptr,ptr);		
	}
	return str;
}

void chk_string_destroy(chk_string *str) {
	if(str->ptr != str->buff)
		free(str->ptr);
	free(str);
}

int32_t chk_string_append(chk_string *str,const char *ptr) {

	char  *tmp;
	size_t len = strlen(ptr);
	size_t newlen = len + str->len;
	if(newlen >= CHK_STRING_DEFAULT_BUFF_SIZE) {
		if(str->ptr == str->buff) {
			tmp = calloc(1,newlen + 1);
		}
		else {
			tmp = realloc(str->ptr,newlen + 1);
		}
		if(!tmp)
			return -1;
		str->ptr = tmp;		
	}
	strcat(str->ptr,ptr);
	str->len = newlen;
	return 0;
}

size_t chk_string_size(chk_string *str) {
	return str->len;
}

int32_t chk_string_equal(chk_string *r,chk_string *l) {
	return strcmp(r->ptr,l->ptr);
}

const char *chk_string_c_str(chk_string *str) {
	return str->ptr;
}