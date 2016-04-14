#include <stdio.h>
#include "chuck.h"
#include "util/chk_string.h"
#include "http/chk_http.h"


int main(){

	chk_http_packet *http_packet = chk_http_packet_new();
	//chk_http_set_url(http_packet,chk_string_new_cstr("www.google.cn"));
	//printf("url:%s\n",chk_http_get_url(http_packet));

	chk_http_set_header(http_packet,chk_string_new_cstr("1"),chk_string_new_cstr("1"));
	chk_http_set_header(http_packet,chk_string_new_cstr("2"),chk_string_new_cstr("2"));
	chk_http_set_header(http_packet,chk_string_new_cstr("3"),chk_string_new_cstr("3"));
	chk_http_set_header(http_packet,chk_string_new_cstr("4"),chk_string_new_cstr("4"));
	chk_http_set_header(http_packet,chk_string_new_cstr("5"),chk_string_new_cstr("5"));
	chk_http_set_header(http_packet,chk_string_new_cstr("6"),chk_string_new_cstr("6"));
	chk_http_set_header(http_packet,chk_string_new_cstr("7"),chk_string_new_cstr("7"));

	chk_http_set_header(http_packet,chk_string_new_cstr("8"),chk_string_new_cstr("8"));
	chk_http_set_header(http_packet,chk_string_new_cstr("9"),chk_string_new_cstr("9"));
	chk_http_set_header(http_packet,chk_string_new_cstr("10"),chk_string_new_cstr("10"));
	chk_http_set_header(http_packet,chk_string_new_cstr("11"),chk_string_new_cstr("11"));
	chk_http_set_header(http_packet,chk_string_new_cstr("12"),chk_string_new_cstr("12"));
	chk_http_set_header(http_packet,chk_string_new_cstr("13"),chk_string_new_cstr("13"));
	chk_http_set_header(http_packet,chk_string_new_cstr("14"),chk_string_new_cstr("14"));

	chk_http_set_header(http_packet,chk_string_new_cstr("15"),chk_string_new_cstr("15"));
	chk_http_set_header(http_packet,chk_string_new_cstr("16"),chk_string_new_cstr("16"));
	chk_http_set_header(http_packet,chk_string_new_cstr("17"),chk_string_new_cstr("17"));
	chk_http_set_header(http_packet,chk_string_new_cstr("18"),chk_string_new_cstr("18"));
	chk_http_set_header(http_packet,chk_string_new_cstr("19"),chk_string_new_cstr("19"));
	chk_http_set_header(http_packet,chk_string_new_cstr("20"),chk_string_new_cstr("20"));
	chk_http_set_header(http_packet,chk_string_new_cstr("21"),chk_string_new_cstr("21"));


	chk_http_header_iterator iterator;
	if(0 == chk_http_header_begin(http_packet,&iterator)) {
		do {
			printf("field:%s,value:%s\n",iterator.field,iterator.value);
		}while(0 == chk_http_header_iterator_next(&iterator));
	}

	chk_http_packet_release(http_packet);

	return 0;
}