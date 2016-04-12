#define _CORE_
#include "http-parser/http_parser.h"
#include "http/chk_http.h"

const char *http_method_name[] =
{
#define XX(num, name, string) #name,
  HTTP_METHOD_MAP(XX)
#undef XX
};

const char *chk_http_method2name(int32_t method) {
	method = method - 1;
	if(method >= 0 && method < (sizeof(http_method_name) / sizeof(http_method_name[0]))) {
		return http_method_name[method];
	}
	return NULL;
}

int32_t chk_http_name2method(const char *name) {
	int32_t i,size;
	size = sizeof(http_method_name) / sizeof(http_method_name[0]);
	for(i = 0; i < size; ++i) {
		if(0 == strcasecmp(name,http_method_name[i])) {
			return i + 1;
		}
	}
	return -1;
}

int32_t chk_http_is_vaild_iterator(chk_http_header_iterator *iterator) {
	if(!iterator) return -1;
	if(!iterator->http_packet || !iterator->current) return -1;
	return 0;
}

int32_t chk_http_header_iterator_next(chk_http_header_iterator *iterator) {
	
	chk_http_hash_item  *next;
	if(!iterator || 0 != chk_http_is_vaild_iterator(iterator))
		return -1;
	next = iterator->current->next;
	if(!next && iterator->curidx < CHK_HTTP_SLOTS_SIZE) {
		for(; iterator->curidx < CHK_HTTP_SLOTS_SIZE;) {
			next = iterator->http_packet->headers[++iterator->curidx];
			if(next) break;
		}	
	}
	if(next) {
		iterator->current = next;
		iterator->field = chk_string_c_str(next->field);
		iterator->value = chk_string_c_str(next->value);
		return 0;
	}
	iterator->http_packet = NULL;
	return -1;
}

int32_t chk_http_header_begin(chk_http_packet *http_packet,chk_http_header_iterator *iterator) {
	
	chk_http_hash_item  *begin;
	if(!http_packet || !iterator)
		return -1;
	iterator->http_packet = NULL;
	iterator->curidx = -1;
	for(; iterator->curidx < CHK_HTTP_SLOTS_SIZE;) {
		begin = http_packet->headers[++iterator->curidx];
		if(begin) break;
	}

	if(!begin) return -1;	

	iterator->http_packet = http_packet;
	iterator->current = begin;
	iterator->field = chk_string_c_str(begin->field);
	iterator->value = chk_string_c_str(begin->value);
	return 0;
}

extern uint64_t burtle_hash(register uint8_t* k,register uint64_t length,register uint64_t level);

chk_http_packet *chk_http_packet_new() {
	chk_http_packet *http_packet = calloc(1,sizeof(*http_packet));
	if(!http_packet) return NULL;
	http_packet->refcount = 1;
	return http_packet;
}

void chk_http_packet_release(chk_http_packet *http_packet) {
	int32_t i;
	chk_http_hash_item *listhead,*next;
	if(!http_packet) return;
	if(0 == chh_atomic_decrease_fetch(&http_packet->refcount)) {
		if(http_packet->status) chk_string_destroy(http_packet->status);
		if(http_packet->url) chk_string_destroy(http_packet->url);
		if(http_packet->body) chk_bytebuffer_del(http_packet->body);
		for(i = 0; i < CHK_HTTP_SLOTS_SIZE; ++i) {
			listhead = http_packet->headers[i];
			while(listhead) {
				next = listhead->next;
				if(listhead->field) chk_string_destroy(listhead->field);
				if(listhead->value) chk_string_destroy(listhead->value);
				free(listhead);
				listhead = next;
			}
		}
		free(http_packet);
	}
}

chk_http_packet *chk_http_packet_retain(chk_http_packet *http_packet) {
	if(!http_packet) return NULL;
	chk_atomic_increase_fetch(&http_packet->refcount);
	return http_packet;
}

int32_t chk_http_get_method(chk_http_packet *http_packet) {
	return http_packet->method;
}

const char *chk_http_get_url(chk_http_packet *http_packet) {
	return http_packet->url ? chk_string_c_str(http_packet->url) : NULL;
}

const char *chk_http_get_status(chk_http_packet *http_packet) {
	return http_packet->status ? chk_string_c_str(http_packet->status) : NULL;
}

const char *chk_http_get_header(chk_http_packet *http_packet,const char *field) {
	chk_http_hash_item *listhead;
	uint64_t hashcode = burtle_hash((uint8_t*)field,strlen(field),(uint64_t)1);
	uint64_t idx = hashcode % CHK_HTTP_SLOTS_SIZE;
	listhead = http_packet->headers[idx];
	while(listhead) {
		if(0 == strcmp(field,(const char *)chk_string_c_str(listhead->field))) {
			return chk_string_c_str(listhead->value);
		}
		listhead = listhead->next;
	}
	return NULL;
}

chk_bytebuffer *chk_http_get_body(chk_http_packet *http_packet) {
	return http_packet->body;
}


void chk_http_set_method(chk_http_packet *http_packet,int32_t method) {
	http_packet->method = method;
}

void chk_http_set_url(chk_http_packet *http_packet,chk_string *url) {
	if(http_packet->url) {
		chk_string_destroy(http_packet->url);
	}
	http_packet->url = url;
}

void chk_http_set_status(chk_http_packet *http_packet,chk_string *status) {
	if(http_packet->status) {
		chk_string_destroy(http_packet->status);
	}
	http_packet->status = status;
}

int32_t chk_http_set_header(chk_http_packet *http_packet,chk_string *field,chk_string *value) {
	chk_http_hash_item *listhead;
	const char *str = chk_string_c_str(field);
	uint64_t hashcode = burtle_hash((uint8_t*)str,strlen(str),(uint64_t)1);
	uint64_t idx = hashcode % CHK_HTTP_SLOTS_SIZE;
	listhead = http_packet->headers[idx];
	while(listhead) {
		if(0 == strcmp(str,(const char *)chk_string_c_str(listhead->field))) {
			break;
		}
		listhead = listhead->next;
	}
	if(!listhead) {
		listhead = calloc(1,sizeof(*listhead));
		if(!listhead) return -1;
		listhead->field = field;
		listhead->value = value;
		//chain the collision
		listhead->next = http_packet->headers[idx];
		http_packet->headers[idx] = listhead;		
	}
	else {
		//replace the old
		chk_string_destroy(listhead->field);
		chk_string_destroy(listhead->value);
		listhead->field = field;
		listhead->value = value;
	}

	return 0;
}

int32_t chk_http_append_body(chk_http_packet *http_packet,const char *str,size_t size) {
	
	size_t len = http_packet->body ? (uint64_t)http_packet->body->datasize : 0;
	
	if(size <= 0) {
		return -1;
	}

	if(len + size > MAX_UINT32) {
		return -1;
	}

	if(!len) {
		if(NULL == (http_packet->body = chk_bytebuffer_new((uint32_t)size))) {
			return -1;
		}
	}

	return chk_bytebuffer_append(http_packet->body,(uint8_t*)str,(uint32_t)size);

}