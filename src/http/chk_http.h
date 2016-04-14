#ifndef _CHK_HTTP_H
#define _CHK_HTTP_H

#include "util/chk_bytechunk.h"
#include "util/chk_string.h"

#define CHK_HTTP_SLOTS_SIZE 63

typedef struct chk_http_hash_item chk_http_hash_item;
typedef struct chk_http_header_iterator chk_http_header_iterator;

struct chk_http_hash_item{
	chk_http_hash_item *next;
	chk_string         *field;
	chk_string         *value;
};

typedef struct {
	uint32_t        		   refcount;
	int32_t                    method;	
	chk_string                *status;
	chk_string                *url;
	chk_bytebuffer            *body;
	chk_http_hash_item        *headers[CHK_HTTP_SLOTS_SIZE];
}chk_http_packet;

struct chk_http_header_iterator {
	int32_t              curidx;
	chk_http_packet     *http_packet;
	chk_http_hash_item  *current;
	const char          *field;
	const char          *value;	
};

const char *chk_http_method2name(int32_t method);
int32_t chk_http_name2method(const char *name);

int32_t chk_http_is_vaild_header_iterator(chk_http_header_iterator *);
int32_t chk_http_header_iterator_next(chk_http_header_iterator *);
int32_t chk_http_header_begin(chk_http_packet*,chk_http_header_iterator*);

chk_http_packet *chk_http_packet_new();
void chk_http_packet_release(chk_http_packet *http_packet);
chk_http_packet *chk_http_packet_retain(chk_http_packet *http_packet);
int32_t chk_http_get_method(chk_http_packet *http_packet);
const char *chk_http_get_url(chk_http_packet *http_packet);
const char *chk_http_get_status(chk_http_packet *http_packet);
const char *chk_http_get_header(chk_http_packet *http_packet,const char *field);
chk_bytebuffer *chk_http_get_body(chk_http_packet *http_packet);
void chk_http_set_method(chk_http_packet *http_packet,int32_t method);
void chk_http_set_url(chk_http_packet *http_packet,chk_string *url);
void chk_http_set_status(chk_http_packet *http_packet,chk_string *status);
int32_t chk_http_set_header(chk_http_packet *http_packet,chk_string *field,chk_string *value);
int32_t chk_http_append_body(chk_http_packet *http_packet,const char *str,size_t size);



/*
typedef struct {
	chk_http_packet *http_packet;	
}chk_http_request;

typedef struct {
	chk_http_packet *http_packet;
}chk_http_response;

chk_http_request *chk_http_request_new(chk_http_packet *http_packet);
void chk_http_request_destroy(chk_http_request *);
chk_http_request *chk_http_request_clone(chk_http_request *);

int32_t chk_http_request_get_method(chk_http_request *);
const char *chk_http_request_get_url(chk_http_request *);
const char *chk_http_request_get_status(chk_http_request *);
const char *chk_http_request_get_header(chk_http_request *,const char *field);
chk_bytebuffer *chk_http_request_get_body(chk_http_request *);


chk_http_response *chk_http_response_new();
void chk_http_response_destroy(chk_http_response *);
void chk_http_response_set_method(chk_http_response *,int32_t method);
void chk_http_response_set_url(chk_http_response *,chk_string *url);
void chk_http_response_set_status(chk_http_response *,chk_string *status);
int32_t chk_http_response_set_header(chk_http_response *,chk_string *field,chk_string *value);
int32_t chk_http_response_add_body(chk_http_response *,const char *str,size_t size);
*/


#endif