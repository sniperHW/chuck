#include "http/chk_http.h"
#include "http-parser/http_parser.h"

#define HTTP_PACKET_METATABLE "lua_http_packet"

#define HTTP_CONNECTION_METATABLE "lua_http_connection"

typedef struct {
	http_parser           parser;
    http_parser_settings  settings;  
    chk_http_packet      *packet;
	chk_string           *field;
	chk_string           *value;	
	chk_string           *status;
	chk_string           *url;
	chk_stream_socket    *socket;
	chk_luaRef            cb;
	int32_t               method;
	int32_t               errcode;
	uint32_t              max_header_size;
	uint32_t              max_content_size;
	uint32_t              total_packet_size;	
}http_connection;

typedef struct {
	chk_http_packet *packet;
}lua_http_packet;

#define lua_check_http_packet(L,I)	\
	(lua_http_packet*)luaL_checkudata(L,I,HTTP_PACKET_METATABLE)

#define lua_check_http_connection(L,I)	\
	(http_connection*)luaL_checkudata(L,I,HTTP_CONNECTION_METATABLE)

static int32_t lua_http_connection_gc(lua_State *L) {
	http_connection *conn = lua_check_http_connection(L,1);
	if(conn->packet) chk_http_packet_release(conn->packet);
	if(conn->url) chk_string_destroy(conn->url);
	if(conn->status) chk_string_destroy(conn->status);
	if(conn->field) chk_string_destroy(conn->field);
	if(conn->value) chk_string_destroy(conn->value);			
	if(conn->socket) {
		chk_stream_socket_close(conn->socket,0);
		chk_stream_socket_setUd(conn->socket,NULL);
	}
	chk_luaRef_release(&conn->cb);
	return 0;
}

static int32_t lua_http_packet_gc(lua_State *L) {
	lua_http_packet *packet = lua_check_http_packet(L,1);
	chk_http_packet_release(packet->packet);
	return 0;
}
	
static int on_message_begin(http_parser *parser)
{
	http_connection *conn = (http_connection*)parser;
	if(conn->packet) chk_http_packet_release(conn->packet);
	conn->packet = chk_http_packet_new();
	conn->total_packet_size = 0;
	if(conn->field) chk_string_destroy(conn->field);
	if(conn->value) chk_string_destroy(conn->value);
	if(conn->url) chk_string_destroy(conn->url);
	if(conn->status) chk_string_destroy(conn->status);	
	conn->url = conn->status = conn->field = conn->value = NULL;
	return 0;
}

#define CHECK_HEADER_SIZE(CONN,LEN)							\
do{															\
	CONN->total_packet_size += LEN;							\
	if(CONN->total_packet_size > CONN->max_header_size) {	\
		CONN->errcode = CHK_EHTTPHEADER;					\
		return -1;											\
	}														\
}while(0);

static int on_url(http_parser *parser, const char *at, size_t length)
{	
	http_connection *conn = (http_connection*)parser;
	if(conn->url)
		chk_string_append(conn->url,at,length);
	else
		conn->url = chk_string_new(at,length);
	CHECK_HEADER_SIZE(conn,length);
	return 0;
}

static int on_status(http_parser *parser, const char *at, size_t length)
{
	http_connection *conn = (http_connection*)parser;
	if(conn->status)
		chk_string_append(conn->status,at,length);
	else
		conn->status = chk_string_new(at,length);
	CHECK_HEADER_SIZE(conn,length);	
	return 0;			
}

static int on_header_field(http_parser *parser, const char *at, size_t length)
{
	http_connection *conn = (http_connection*)parser;
	size_t i;
	char   *ptr = (char*)at;
	//将field转成小写
	for(i = 0; i < length; ++i) {
		if(ptr[i] >= 'A' && ptr[i] <= 'Z') {
			ptr[i] += ('a'-'A');
		}
	}
	if(!conn->value && conn->field)
		chk_string_append(conn->field,at,length);
	else {
		if(conn->value) {
			chk_http_set_header(conn->packet,conn->field,conn->value);
			conn->value = NULL;
		}
		conn->field = chk_string_new(at,length);
	}
	CHECK_HEADER_SIZE(conn,length);	
	return 0;				
}

static int on_header_value(http_parser *parser, const char *at, size_t length)
{
	http_connection *conn = (http_connection*)parser;
	if(conn->value)
		chk_string_append(conn->value,at,length);
	else
		conn->value = chk_string_new(at,length);
	CHECK_HEADER_SIZE(conn,length);	
	return 0;				
}

static int on_headers_complete(http_parser *parser)
{	
	http_connection *conn = (http_connection*)parser;

	if(conn->field && conn->value) {
		chk_http_set_header(conn->packet,conn->field,conn->value);
		conn->field = conn->value = NULL;
	}

	//检查是否有Content-Length,长度是否超限制
	const char *content_length = chk_http_get_header(conn->packet,"content-length");
	if(content_length && atol(content_length) > conn->max_content_size) {
		conn->errcode = CHK_EHTTPCONTENT;
		return -1;
	}
	return 0;		
}

static int on_body(http_parser *parser, const char *at, size_t length)
{
	http_connection *conn = (http_connection*)parser;
	if(0 != chk_http_append_body(conn->packet,at,length)){
		conn->errcode = CHK_EHTTPCONTENT;
		return -1;
	}
	return 0;
}

typedef struct {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
	chk_http_packet *packet;
}lua_http_packet_PushFunctor;

void lua_push_http_packet(chk_luaPushFunctor *self,lua_State *L) {
	lua_http_packet_PushFunctor *pushFunctor = (lua_http_packet_PushFunctor*)self;
	lua_http_packet *lua_packet = (lua_http_packet*)lua_newuserdata(L, sizeof(*lua_packet));
	lua_packet->packet = chk_http_packet_retain(pushFunctor->packet);
	luaL_getmetatable(L, HTTP_PACKET_METATABLE);
	lua_setmetatable(L, -2);	
}

static int on_message_complete(http_parser *parser)
{	
	const char   *error; 
	lua_http_packet_PushFunctor packet;
	http_connection *conn = (http_connection*)parser;
	chk_http_set_method(conn->packet,conn->method);
	chk_http_set_url(conn->packet,conn->url);
	chk_http_set_status(conn->packet,conn->status);

	if(conn->cb.L) {
		packet.packet = conn->packet;
		packet.Push = lua_push_http_packet;
		if(NULL != (error = chk_Lua_PCallRef(conn->cb,"f",&packet))) {
			CHK_SYSLOG(LOG_ERROR,"error on_message_complete %s",error);
		}
	}

	chk_http_packet_release(conn->packet);
	conn->url = NULL;
	conn->status = NULL;
	conn->packet = NULL;

	return 0;						
}

static void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	size_t      nparsed;
	char       *buff;
	const char *errmsg;
	uint32_t    spos,size_remain,size;
	chk_bytechunk *chunk;
	http_connection *conn = (http_connection*)chk_stream_socket_getUd(s);
	if(!conn) return;
	if(data) {
		chunk = data->head;
		spos  = data->spos;
		size_remain = data->datasize;
		do{
			buff = chunk->data + spos;
			size = chunk->cap - spos;
			if(size > size_remain) size = size_remain;
			nparsed = http_parser_execute(&conn->parser,&conn->settings,buff,size);
			if(nparsed > 0) {			
				size_remain -= nparsed;
				spos += nparsed;
				if(spos >= chunk->cap){	
					spos = 0;
					chunk = chunk->next;
				}
			}else {
				error = conn->errcode;
				break;
			}
		}while(size_remain);
	}

	if(!data || error){
		//用nil调用lua callback,通告连接断开
		if(NULL != (errmsg = chk_Lua_PCallRef(conn->cb,":"))) {
			CHK_SYSLOG(LOG_ERROR,"error data_event_cb %s",errmsg);
		}
	}	
}

static int32_t lua_new_http_connection(lua_State *L) {
	int32_t  fd;
	uint32_t max_header_size,max_content_size;
	chk_stream_socket *s;
	http_connection   *conn;
	chk_stream_socket_option option = {
		.decoder = NULL,
		.recv_buffer_size = 4096
	};
	fd = (int32_t)luaL_checkinteger(L,1);
	max_header_size = (uint32_t)luaL_checkinteger(L,2);
	max_content_size = (uint32_t)luaL_checkinteger(L,3);
	s = chk_stream_socket_new(fd,&option);
	if(!s) {
		return 0;
	}

	conn = (http_connection*)lua_newuserdata(L, sizeof(*conn));
	if(!conn) {
		chk_stream_socket_close(s,1);
		return 0;
	}

	if(max_header_size > MAX_UINT32/2) max_header_size = MAX_UINT32/2;
	if(max_content_size > MAX_UINT32/2) max_content_size = MAX_UINT32/2;
	memset(conn,0,sizeof(*conn));
	conn->settings.on_message_begin = on_message_begin;
	conn->settings.on_url = on_url;
	conn->settings.on_status = on_status;
	conn->settings.on_header_field = on_header_field;
	conn->settings.on_header_value = on_header_value;
	conn->settings.on_headers_complete = on_headers_complete;
	conn->settings.on_body = on_body;
	conn->settings.on_message_complete = on_message_complete;
	conn->socket = s;
	conn->max_header_size = max_header_size;
	conn->max_content_size = max_content_size;
	chk_stream_socket_setUd(s,conn);
	http_parser_init(&conn->parser,HTTP_BOTH);
	luaL_getmetatable(L, HTTP_CONNECTION_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int32_t lua_http_connection_bind(lua_State *L) {
	chk_event_loop    *event_loop;
	http_connection   *conn;
	chk_stream_socket *s;
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of http_connection_bind must be lua function");
	event_loop = lua_checkeventloop(L,2);
	conn       = lua_check_http_connection(L,1);
	s          = conn->socket;
	if(!s) return luaL_error(L,"http connection is close");
	conn->cb   = chk_toluaRef(L,3);
	if(0 != chk_loop_add_handle(event_loop,(chk_handle*)s,data_event_cb)) {
		lua_pushstring(L,"http_connection_bind failed");
		return 1;
	}
	return 0;
}

static void write_http_header(chk_bytebuffer *b,chk_http_packet *packet) {
	chk_http_header_iterator iterator;
	if(0 == chk_http_header_begin(packet,&iterator)) {
		do {
			chk_bytebuffer_append(b,(uint8_t*)iterator.field,strlen(iterator.field));
			chk_bytebuffer_append(b,(uint8_t*)" : ",strlen(" : "));
			chk_bytebuffer_append(b,(uint8_t*)iterator.value,strlen(iterator.value));
			chk_bytebuffer_append(b,(uint8_t*)"\r\n",strlen("\r\n"));
		}while(0 == chk_http_header_iterator_next(&iterator));
	}	
}

static void write_http_body(chk_bytebuffer *b,chk_http_packet *packet) {
	char body_length[64];
	uint32_t spos,chunk_data_size,size_remain;
	chk_bytechunk  *head;	
	chk_bytebuffer *body = chk_http_get_body(packet);
	do {
		if(!body || !body->head || 0 == body->datasize)
			break;
		head = body->head;
		snprintf(body_length,64,"%u",body->datasize);
		chk_bytebuffer_append(b,(uint8_t*)"Content-Length: ",strlen("Content-Length: "));
		chk_bytebuffer_append(b,(uint8_t*)body_length,strlen(body_length));
		chk_bytebuffer_append(b,(uint8_t*)" \r\n\r\n",strlen(" \r\n\r\n"));
		//write the content
		spos = body->spos;
		size_remain = body->datasize;
		do{
			chunk_data_size = head->cap - spos;	
			if(chunk_data_size > size_remain) chunk_data_size = size_remain;				
			chk_bytebuffer_append(b,(uint8_t*)&head->data[spos],chunk_data_size);
			head = head->next;
			if(!head) break;
			size_remain -= chunk_data_size;
			spos = 0;		
		}while(size_remain);
		return;		
	}while(0);
	chk_bytebuffer_append(b,(uint8_t*)"\r\n",strlen("\r\n"));
}

static void push_bytebuffer(lua_State *L,chk_bytebuffer *buffer) {
	uint32_t spos,chunk_data_size,size_remain;
	struct luaL_Buffer b;
	char *in;
	chk_bytechunk   *head;
#if LUA_VERSION_NUM >= 503
	in = luaL_buffinitsize(L, &b, (size_t)buffer->datasize);
#else
 	luaL_buffinit(L, &b);
 	in = luaL_prepbuffsize(&b,(size_t)buffer->datasize);
#endif
	head = buffer->head;
	spos = buffer->spos;
	size_remain = buffer->datasize;
	do{
		chunk_data_size = head->cap - spos;	
		if(chunk_data_size > size_remain) chunk_data_size = size_remain;				
		memcpy(in,&head->data[spos],(size_t)chunk_data_size);
		head = head->next;
		if(!head) break;
		in += chunk_data_size;
		size_remain -= chunk_data_size;
		spos = 0;		
	}while(size_remain);
#if LUA_VERSION_NUM >= 503
	luaL_pushresultsize(&b,(size_t)buffer->datasize);
#else
	luaL_addsize(&b,(size_t)buffer->datasize);	
	luaL_pushresult(&b);
#endif	
}

static int32_t lua_http_connection_send_request(lua_State *L) {	
	http_connection   *conn;
	lua_http_packet   *packet;
	chk_bytebuffer    *b;	
	const char        *http_version,*path,*host,*method;
	conn   = lua_check_http_connection(L,1);
	if(!conn->socket) {
		lua_pushstring(L,"http connection is close");
		return 1;
	}
	http_version = luaL_checkstring(L,2);
	host = luaL_checkstring(L,3);
	method = luaL_checkstring(L,4);
	path = luaL_checkstring(L,5);
	packet = lua_check_http_packet(L,6);	
	b = chk_bytebuffer_new(4096);
	if(!b) {
		lua_pushstring(L,"new buffer failed");
		return 1;
	}

	chk_bytebuffer_append(b,(uint8_t*)method,strlen(method));
	chk_bytebuffer_append(b,(uint8_t*)" ",strlen(" "));
	chk_bytebuffer_append(b,(uint8_t*)path,strlen(path));
	chk_bytebuffer_append(b,(uint8_t*)" HTTP/",strlen(" HTTP/"));
	chk_bytebuffer_append(b,(uint8_t*)http_version,strlen(http_version));
	chk_bytebuffer_append(b,(uint8_t*)"\r\n",strlen("\r\n"));
	chk_bytebuffer_append(b,(uint8_t*)"Host: ",strlen("Host: "));
	chk_bytebuffer_append(b,(uint8_t*)host,strlen(host));		
	chk_bytebuffer_append(b,(uint8_t*)"\r\n",strlen("\r\n"));
	
	write_http_header(b,packet->packet);
	write_http_body(b,packet->packet);	
	if(0 != chk_stream_socket_send(conn->socket,b)){
		lua_pushstring(L,"send http request failed");
		return 1;
	}	
	//调试用
	//push_bytebuffer(L,b);
	return 0;
}

static int32_t lua_http_connection_send_response(lua_State *L) {
	http_connection   *conn;
	lua_http_packet   *packet;
	chk_bytebuffer    *b;	
	const char        *http_version,*status,*phase;
	conn   = lua_check_http_connection(L,1);
	if(!conn->socket) {
		lua_pushstring(L,"http connection is close");
		return 1;
	}	
	http_version = luaL_checkstring(L,2);
	status = luaL_checkstring(L,3);
	phase = luaL_checkstring(L,4);
	packet = lua_check_http_packet(L,5);	
	b = chk_bytebuffer_new(4096);
	if(!b) {
		lua_pushstring(L,"new buffer failed");
		return 1;
	}
	chk_bytebuffer_append(b,(uint8_t*)"HTTP/",strlen("HTTP/"));
	chk_bytebuffer_append(b,(uint8_t*)http_version,strlen(http_version));
	chk_bytebuffer_append(b,(uint8_t*)" ",strlen(" "));
	chk_bytebuffer_append(b,(uint8_t*)status,strlen(status));
	chk_bytebuffer_append(b,(uint8_t*)" ",strlen(" "));
	chk_bytebuffer_append(b,(uint8_t*)phase,strlen(phase));
	chk_bytebuffer_append(b,(uint8_t*)"\r\n",strlen("\r\n"));						
	write_http_header(b,packet->packet);
	if(!chk_http_get_body(packet->packet))
		chk_bytebuffer_append(b,(uint8_t*)"Content-Length:0\r\n\r\n",strlen("Content-Length:0\r\n\r\n"));
	else	
		write_http_body(b,packet->packet);
	if(0 != chk_stream_socket_send(conn->socket,b)){
		lua_pushstring(L,"send http response failed");
		return 1;
	}
	//调试用	
	//push_bytebuffer(L,b);
	return 0;
}

static int32_t lua_http_connection_close(lua_State *L) {
	http_connection   *conn = lua_check_http_connection(L,1);
	if(conn->socket) {
		chk_stream_socket_setUd(conn->socket,NULL);
		chk_stream_socket_close(conn->socket,0);
		conn->socket = NULL;
	}
	return 0;
}

static int32_t lua_http_packet_get_method(lua_State *L) {
	lua_http_packet *packet = lua_check_http_packet(L,1);
	const char *method = chk_http_method2name(chk_http_get_method(packet->packet));
	if(!method) return 0;
	lua_pushstring(L,method);
	return 1;
}

static int32_t lua_http_packet_get_status(lua_State *L) {
	lua_http_packet *packet = lua_check_http_packet(L,1);
	const char *status = chk_http_get_status(packet->packet);
	if(!status) return 0;
	lua_pushstring(L,status);
	return 1;
}

static int32_t lua_http_packet_get_url(lua_State *L) {
	lua_http_packet *packet = lua_check_http_packet(L,1);
	const char *url = chk_http_get_url(packet->packet);
	if(!url) return 0;
	lua_pushstring(L,url);
	return 1;	
}

static int32_t lua_http_packet_get_body(lua_State *L) {

	lua_http_packet *packet = lua_check_http_packet(L,1);
	chk_bytebuffer  *body   = chk_http_get_body(packet->packet);
	if(!body || !body->head) return 0;
	push_bytebuffer(L,body);
	return 1;
}

static int32_t lua_http_packet_get_header(lua_State *L) {
	lua_http_packet *packet = lua_check_http_packet(L,1);
	const char *field = luaL_checkstring(L,2);
	const char *value = chk_http_get_header(packet->packet,field);	
	if(!value) return 0;
	lua_pushstring(L,value);
	return 1;
}

static int32_t lua_http_packet_get_headers(lua_State *L) {
	int32_t c;
	chk_http_header_iterator iterator;
	lua_http_packet *packet = lua_check_http_packet(L,1);
	if(0 == chk_http_header_begin(packet->packet,&iterator)) {
		printf("here\n");
		//{{f,v},{f,v}...}
		lua_newtable(L);
		c = 1;
		do {
			lua_newtable(L);
			lua_pushstring(L,iterator.field);
			lua_rawseti(L,-2,1);
			lua_pushstring(L,iterator.value);
			lua_rawseti(L,-2,2);
			lua_rawseti(L,-2,c++);
		}while(0 == chk_http_header_iterator_next(&iterator));
		return 1;
	}
	return 0;
}

static int32_t lua_http_packet_set_header(lua_State *L) {
	lua_http_packet *packet = lua_check_http_packet(L,1);
	const char *field = luaL_checkstring(L,2);
	const char *value = luaL_checkstring(L,3);
	chk_string *str_field = chk_string_new(field,strlen(field)); 
	chk_string *str_value = chk_string_new(value,strlen(value)); 
	chk_http_set_header(packet->packet,str_field,str_value);
	return 0;
}

static int32_t lua_http_packet_append_body(lua_State *L) {
	size_t len;
	lua_http_packet *packet = lua_check_http_packet(L,1);
	const char *str = luaL_checklstring(L,2,&len);
	chk_http_append_body(packet->packet,str,len);
	return 0;
}

static int32_t lua_http_packet_new(lua_State *L) {
	lua_http_packet *packet = (lua_http_packet*)lua_newuserdata(L, sizeof(*packet));
	if(!packet)
		return 0;
	packet->packet = chk_http_packet_new();
	if(!packet->packet)
		return 0;
	luaL_getmetatable(L, HTTP_PACKET_METATABLE);
	lua_setmetatable(L, -2);
	return 1;	
}

static void register_http(lua_State *L) {
	luaL_Reg http_packet_mt[] = {
		{"__gc", lua_http_packet_gc},
		{NULL, NULL}
	};

	luaL_Reg http_packet_methods[] = {
		{"GetMethod",    lua_http_packet_get_method},
		{"GetStatus",    lua_http_packet_get_status},
		{"GetURL",       lua_http_packet_get_url},
		{"GetBody",      lua_http_packet_get_body},
		{"GetHeader",    lua_http_packet_get_header},
		{"GetAllHeaders",lua_http_packet_get_headers},
		{"SetHeader",    lua_http_packet_set_header},
		{"AppendBody",	 lua_http_packet_append_body},															
		{NULL,     NULL}
	};

	luaL_Reg http_connection_mt[] = {
		{"__gc", lua_http_connection_gc},
		{NULL, NULL}
	};

	luaL_Reg http_connection_methods[] = {
		{"SendRequest",    lua_http_connection_send_request},
		{"SendResponse",   lua_http_connection_send_response},
		{"Close",   	   lua_http_connection_close},
		{"Bind",           lua_http_connection_bind},		
		{NULL,     NULL}
	};

	luaL_newmetatable(L, HTTP_PACKET_METATABLE);
	luaL_setfuncs(L, http_packet_mt, 0);

	luaL_newlib(L, http_packet_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	luaL_newmetatable(L, HTTP_CONNECTION_METATABLE);
	luaL_setfuncs(L, http_connection_mt, 0);

	luaL_newlib(L, http_connection_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	lua_newtable(L);	
	SET_FUNCTION(L,"Connection",lua_new_http_connection);
	SET_FUNCTION(L,"HttpPacket",lua_http_packet_new);
}


