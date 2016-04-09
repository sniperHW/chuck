#ifndef _CHK_HTTP_H
#define _CHK_HTTP_H

#include "http-parser/http_parser.h"
#include "uitl/chk_bytechunk.h"

typedef struct {
	//std::vector<std::string> m_header_field;
	//std::vector<std::string> m_header_value;	
	//std::string              m_status;
	chk_bytebuffer            *body;
	uint32_t        		   refcount;
	int32_t                    method;
}chk_http_packet;


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
int32_t chk_http_response_set_method(chk_http_response *,int32_t method);
int32_t chk_http_response_set_url(chk_http_response *,const char *url);
int32_t chk_http_response_set_status(chk_http_response *,const char *status);
int32_t chk_http_response_set_header(chk_http_response *,const char *field,const char *value);
int32_t chk_http_response_add_body(chk_http_response *,const char *str,size_t size);


#endif