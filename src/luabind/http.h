#include "util/chk_http.h"
#include "http_parser/http_parser.h"

#define HTTP_PACKET_METATABLE "lua_http_packet"

#define HTTP_CONNECTION_METATABLE "lua_http_connection"

enum{
	HTTP_BODY_TOO_LARGE = -1,
};

typedef struct {
	http_parser           parser;
    http_parser_settings  settings;
	int32_t               method;
	int32_t               errcode;  
    chk_http_packet      *packet;
	chk_string           *field;
	chk_string           *value;	
	chk_string           *status;
	chk_string           *url;
	chk_stream_socket    *socket;
	chk_luaRef            cb;
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
	if(conn->socket) chk_stream_socket_close(conn->socket,0);
	if(conn->cb) chk_luaRef_release(&conn->cb);
	return 0;
}

static int32_t lua_http_packet_gc(lua_State *L) {
	lua_http_packet *packet = lua_check_read_packet(L,1);
	chk_http_packet_release(packet->packet);
	return 0;
}
	
static int on_message_begin(http_parser *parser)
{
	http_connection *conn = (http_connection*)parser;
	if(conn->packet) chk_http_packet_release(conn->packet);
	conn->packet = chk_http_packet_new();
	return 0;
}

static int on_url(http_parser *parser, const char *at, size_t length)
{	
	http_connection *conn = lua_check_http_connection(L,1);
	if(conn->url)
		chk_string_append(conn->url,at,length);
	else
		conn->url = chk_string_new(at,length);
	return 0;
}

static int on_status(http_parser *parser, const char *at, size_t length)
{
	http_connection *conn = lua_check_http_connection(L,1);
	if(conn->status)
		chk_string_append(conn->status,at,length);
	else
		conn->status = chk_string_new(at,length);
	return 0;			
}

static int on_header_field(http_parser *parser, const char *at, size_t length)
{
	http_connection *conn = lua_check_http_connection(L,1);
	if(conn->field)
		chk_string_append(conn->field,at,length);
	else {
		if(conn->field) {
			chk_http_set_header(conn->packet,conn->field,conn->value);
			conn->field = conn->value = NULL;
		}
		conn->field = chk_string_new(at,length);
	}
	return 0;				
}

static int on_header_value(http_parser *parser, const char *at, size_t length)
{
	http_connection *conn = lua_check_http_connection(L,1);
	if(conn->value)
		chk_string_append(conn->value,at,length);
	else
		conn->value = chk_string_new(at,length);
	return 0;				
}

static int on_headers_complete(http_parser *parser)
{	
	return 0;		
}

static int on_body(http_parser *parser, const char *at, size_t length)
{
	http_connection *conn = lua_check_http_connection(L,1);
	if(0 != chk_http_append_body(conn->packet,at,length)){
		conn->errcode = HTTP_BODY_TOO_LARGE;
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
	http_connection *conn = lua_check_http_connection(L,1);
	chk_http_set_method(conn->packet,conn->method);
	chk_http_set_url(conn->packet,conn->url);
	chk_http_set_status(conn->packet,conn->status);

	if(conn->cb) {
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
	size_t   nparsed;
	char    *buff;
	http_connection *conn = (http_connection*)chk_stream_socket_getUd(s);
	if(data) {
		buff = data->head->data;	
		do{
			nparsed = http_parser_execute(&conn->parser,&conn->settings,&buff[data->spos],data->datasize);
			if(nparsed > 0) {			
				data->spos     += nparsed;
				data->datasize -= nparsed;
			}else {
				error = conn->errcode;
				break;
			}
		}while(data->datasize > 0);
	}
	if(error){
		chk_stream_socket_close(s,1);
		conn->socket = NULL;
	}	
}

static int32_t lua_http_connection_bind(lua_State *L) {
	http_connection   *conn;
	chk_event_loop    *event_loop;
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of http_connection_bind must be lua function");
	conn = lua_check_http_connection(L,1);
	event_loop = lua_checkeventloop(L,2);
	conn->cb = chk_toluaRef(L,3);
	if(0 != chk_loop_add_handle(event_loop,(chk_handle*)conn->socket,data_cb)) {
		lua_pushstring(L,"http_connection bind failed");
		return 1;
	}
	return 0;
}

static int32_t lua_new_http_connection(lua_State *L) {
	int32_t fd;
	chk_stream_socket *s;
	http_connection   *conn;
	chk_stream_socket_option option = {
		.decoder = NULL,
		.recv_buffer_size = 4096
	};
	fd = (int32_t)luaL_checkinteger(L,1);
	s = chk_stream_socket_new(fd,&option);
	if(!s) {
		return 0;
	}

	conn = (http_connection*)lua_newuserdata(L, sizeof(*conn));
	if(!conn) {
		chk_stream_socket_close(s);
		return 0;
	}
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
	chk_stream_socket_setUd(s,conn);
	http_parser_init(&conn->parser,HTTP_BOTH);
	luaL_getmetatable(L, HTTP_CONNECTION_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

int32_t lua_http_connection_send_request(lua_State *L) {
	return 0;
}

int32_t lua_http_connection_send_response(lua_State *L) {
	return 0;
}

int32_t lua_http_connection_close(lua_State *L) {
	return 0;
}

int32_t lua_http_packet_get_method(lua_State *L) {
	return 0;
}

int32_t lua_http_packet_get_status(lua_State *L) {
	return 0;
}

int32_t lua_http_packet_get_url(lua_State *L) {
	return 0;
}

int32_t lua_http_packet_get_body(lua_State *L) {
	return 0;
}

int32_t lua_http_packet_get_header(lua_State *L) {
	return 0;
}

int32_t lua_http_packet_get_headers(lua_State *L) {
	return 0;
}

int32_t lua_http_packet_set_header(lua_State *L) {
	return 0;
}

int32_t lua_http_packet_append_body(lua_State *L) {
	return 0;
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


	/*

	lua_newtable(L);

	lua_pushstring(L,"stream");
	lua_newtable(L);
	
	SET_FUNCTION(L,"New",lua_stream_socket_new);

	lua_pushstring(L,"ip4");
	lua_newtable(L);
	SET_FUNCTION(L,"dail",lua_dail_ip4);
	SET_FUNCTION(L,"listen",lua_listen_ip4);
	lua_settable(L,-3);

	lua_settable(L,-3);

	SET_FUNCTION(L,"closefd",lua_close_fd);
	
	*/

}


