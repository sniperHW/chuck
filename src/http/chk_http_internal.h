#define _CORE_

extern uint64_t burtle_hash(register uint8_t* k,register uint64_t length,register uint64_t level);

static inline int32_t chk_http_get_method(chk_http_packet *http_packet) {
	return http_packet->method;
}

static inline const char *chk_http_get_url(chk_http_packet *http_packet) {
	return http_packet->url ? chk_string_c_str(http_packet->url) : NULL;
}

static inline const char *chk_http_get_status(chk_http_packet *http_packet) {
	return http_packet->status ? chk_string_c_str(http_packet->status) : NULL;
}

static inline const char *chk_http_get_header(chk_http_packet *http_packet,const char *field) {
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

static inline chk_bytebuffer *chk_http_get_body(chk_http_packet *http_packet) {
	return http_packet->body;
}


static inline void chk_http_set_method(chk_http_packet *http_packet,int32_t method) {
	http_packet->method = method;
}

static inline void chk_http_set_url(chk_http_packet *http_packet,chk_string *url) {
	if(http_packet->url) {
		chk_string_destroy(http_packet->url);
	}
	http_packet->url = url;
}

static inline void chk_http_set_status(chk_http_packet *http_packet,chk_string *status) {
	if(http_packet->status) {
		chk_string_destroy(http_packet->status);
	}
	http_packet->status = status;
}

static inline int32_t chk_http_set_header(chk_http_packet *http_packet,chk_string *field,chk_string *value) {
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

static inline int32_t chk_http_add_body(chk_http_packet *http_packet,const char *str,size_t size) {
	
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




