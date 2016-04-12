#include <stdio.h>
#include "chuck.h"
#include "util/chk_string.h"
#include "http/chk_http.h"
#define _CORE_
#include "http/chk_http_internal.h"

int main(){

	chk_http_packet *http_packet = chk_http_packet_new();
	chk_http_set_url(http_packet,chk_string_new("www.google.cn"));
	printf("url:%s\n",chk_http_get_url(http_packet));

	chk_http_set_header(http_packet,chk_string_new("1"),chk_string_new("1"));
	chk_http_set_header(http_packet,chk_string_new("2"),chk_string_new("2"));
	chk_http_set_header(http_packet,chk_string_new("3"),chk_string_new("3"));
	chk_http_set_header(http_packet,chk_string_new("4"),chk_string_new("4"));
	chk_http_set_header(http_packet,chk_string_new("5"),chk_string_new("5"));
	chk_http_set_header(http_packet,chk_string_new("6"),chk_string_new("6"));
	chk_http_set_header(http_packet,chk_string_new("7"),chk_string_new("7"));

	chk_http_set_header(http_packet,chk_string_new("8"),chk_string_new("8"));
	chk_http_set_header(http_packet,chk_string_new("9"),chk_string_new("9"));
	chk_http_set_header(http_packet,chk_string_new("10"),chk_string_new("10"));
	chk_http_set_header(http_packet,chk_string_new("11"),chk_string_new("11"));
	chk_http_set_header(http_packet,chk_string_new("12"),chk_string_new("12"));
	chk_http_set_header(http_packet,chk_string_new("13"),chk_string_new("13"));
	chk_http_set_header(http_packet,chk_string_new("14"),chk_string_new("14"));

	chk_http_set_header(http_packet,chk_string_new("15"),chk_string_new("15"));
	chk_http_set_header(http_packet,chk_string_new("16"),chk_string_new("16"));
	chk_http_set_header(http_packet,chk_string_new("17"),chk_string_new("17"));
	chk_http_set_header(http_packet,chk_string_new("18"),chk_string_new("18"));
	chk_http_set_header(http_packet,chk_string_new("19"),chk_string_new("19"));
	chk_http_set_header(http_packet,chk_string_new("20"),chk_string_new("20"));
	chk_http_set_header(http_packet,chk_string_new("21"),chk_string_new("21"));


	chk_http_header_iterator iterator;
	if(0 == chk_http_header_begin(http_packet,&iterator)) {
		do {
			printf("field:%s,value:%s\n",iterator.field,iterator.value);
		}while(0 == chk_http_header_iterator_next(&iterator));
	}

	chk_http_packet_release(http_packet);

	return 0;
}