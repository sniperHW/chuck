#define _CORE_

static inline int32_t chk_http_get_method(chk_http_packet *http_packet);
static inline const char *chk_http_get_url(chk_http_packet *http_packet);
static inline const char *chk_http_get_status(chk_http_packet *http_packet);
static inline const char *chk_http_get_header(chk_http_packet *http_packet,const char *field);
static inline chk_bytebuffer *chk_http_get_body(chk_http_packet *http_packet);


static inline int32_t chk_http_set_method(chk_http_packet *http_packet,int32_t method);
static inline int32_t chk_http_set_url(chk_http_packet *http_packet,const char *url);
static inline int32_t chk_http_set_status(chk_http_packet *http_packet,const char *status);
static inline int32_t chk_http_set_header(chk_http_packet *http_packet,const char *field,const char *value);
static inline int32_t chk_http_add_body(chk_http_packet *http_packet,const char *str,size_t size);




