#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util/string.h"
#include "util/list.h"
#include "util/bytebuffer.h"
#include "packet/rawpacket.h"
#include "socket/socket_helper.h"
#include "socket/connector.h"
#include "db/redis/protocol.h"
#include "socket/wrap/wrap_comm.h"
#include "engine/engine.h"

enum{
	SENDING   = SOCKET_END << 2,
};

#define RECV_BUFFSIZE 1024*16

enum{
	CB_C,
	CB_LUA,
};

typedef struct{
	listnode node;
	void     (*cb)(redis_conn*,redisReply*,void *ud);
	union{
		void    *ud;
		luaRef   luacb;//for lua callback
	};
	char     type;
}reply_cb;

typedef struct redis_conn{
    stream_socket_      base;
    struct       		iovec wrecvbuf[1];
    char         		recvbuf[RECV_BUFFSIZE]; 
    struct       		iovec wsendbuf[MAX_WBAF];
    iorequest    		send_overlap;
    iorequest    		recv_overlap;       
    list         		send_list;
    list         		waitreplys;
    parse_tree         *tree;
    void                (*clear)(void*ud);
    void         	    (*on_disconnected)(struct redis_conn*,int32_t err);
    luaRef              lua_disconn_cb;
}redis_conn;

void 
redis_dctor(void *_)
{
	redis_conn *c = (redis_conn*)_;
	packet   *p;
	reply_cb *stcb;
	if(c->on_disconnected)
		c->on_disconnected(c,0);	
	while((p = (packet*)list_pop(&c->send_list))!=NULL)
		packet_del(p);
	while((stcb = (reply_cb*)list_pop(&c->waitreplys))!=NULL){
		if(c->clear && stcb->ud) c->clear(stcb->ud);
		free(stcb);
	}
	if(c->tree)
		parse_tree_del(c->tree);
	free(c);
}

void 
redis_lua_dctor(void *_)
{
	redis_conn *c = (redis_conn*)_;
	packet   *p;
	reply_cb *stcb;
	if(c->on_disconnected)
		c->on_disconnected(c,0);	
	while((p = (packet*)list_pop(&c->send_list))!=NULL)
		packet_del(p);
	while((stcb = (reply_cb*)list_pop(&c->waitreplys))!=NULL){
		if(stcb->luacb.L)
			release_luaRef(&stcb->luacb);
		free(stcb);
	}
	if(c->tree)
		parse_tree_del(c->tree);
	if(c->lua_disconn_cb.L)
		release_luaRef(&c->lua_disconn_cb);
}


static inline void 
prepare_recv(redis_conn *c)
{
	c->wrecvbuf[0].iov_len = RECV_BUFFSIZE-1;
	c->wrecvbuf[0].iov_base = c->recvbuf;	
	c->recv_overlap.iovec_count = 1;
	c->recv_overlap.iovec = c->wrecvbuf;
}

static inline int32_t 
Send(redis_conn *c,int32_t flag)
{
	int32_t ret = stream_socket_send((stream_socket_*)c,&c->send_overlap,flag);		
	if(ret < 0 && -ret == EAGAIN)
		((socket_*)c)->status |= SENDING;
	return ret; 
}

static inline void 
PostRecv(redis_conn *c)
{
	prepare_recv(c);
	stream_socket_recv((stream_socket_*)c,&c->recv_overlap,IO_POST);		
}

static inline int32_t 
Recv(redis_conn *c)
{
	prepare_recv(c);
	return stream_socket_recv((stream_socket_*)c,&c->recv_overlap,IO_NOW);		
}

static inline iorequest*
prepare_send(redis_conn *c)
{
	int32_t     i = 0;
	packet     *w = (packet*)list_begin(&c->send_list);
	bytebuffer *b;
	iorequest * O = NULL;
	uint32_t    buffer_size,size,pos;
	uint32_t    send_size_remain = MAX_SEND_SIZE;
	while(w && i < MAX_WBAF && send_size_remain > 0)
	{
		pos = ((packet*)w)->spos;
		b =   ((packet*)w)->head;
		buffer_size = ((packet*)w)->len_packet;
		while(i < MAX_WBAF && b && buffer_size && send_size_remain > 0)
		{
			c->wsendbuf[i].iov_base = b->data + pos;
			size = b->size - pos;
			size = size > buffer_size ? buffer_size:size;
			size = size > send_size_remain ? send_size_remain:size;
			buffer_size -= size;
			send_size_remain -= size;
			c->wsendbuf[i].iov_len = size;
			++i;
			b = b->next;
			pos = 0;
		}
		if(send_size_remain > 0) w = (packet*)((listnode*)w)->next;
	}
	if(i){
		c->send_overlap.iovec_count = i;
		c->send_overlap.iovec = c->wsendbuf;
		O = (iorequest*)&c->send_overlap;
	}
	return O;
}

static inline void 
update_send_list(redis_conn *c,int32_t _bytestransfer)
{
	assert(_bytestransfer >= 0);
	packet     *w;
	uint32_t    bytestransfer = (uint32_t)_bytestransfer;
	uint32_t    size;
	do{
		w = (packet*)list_begin(&c->send_list);
		assert(w);
		if((uint32_t)bytestransfer >= ((packet*)w)->len_packet)
		{
			list_pop(&c->send_list);
			bytestransfer -= ((packet*)w)->len_packet;
			packet_del(w);
		}else{
			do{
				size = ((packet*)w)->head->size - ((packet*)w)->spos;
				size = size > (uint32_t)bytestransfer ? (uint32_t)bytestransfer:size;
				bytestransfer -= size;
				((packet*)w)->spos += size;
				((packet*)w)->len_packet -= size;
				if(((packet*)w)->spos >= ((packet*)w)->head->size)
				{
					((packet*)w)->spos = 0;
					((packet*)w)->head = ((packet*)w)->head->next;
				}
			}while(bytestransfer);
		}
	}while(bytestransfer);
}


static void 
SendFinish(redis_conn *c,int32_t bytestransfer)
{
	update_send_list(c,bytestransfer);
	if(((socket_*)c)->status & SOCKET_CLOSE)
			return;
	if(!prepare_send(c)) {
		((socket_*)c)->status ^= SENDING;
		return;
	}
	Send(c,IO_POST);		
}

static inline void 
_close(redis_conn *c,int32_t err)
{
	((socket_*)c)->status |= SOCKET_CLOSE;
	engine_remove((handle*)c);
	if(c->on_disconnected){
		c->on_disconnected(c,err);
		c->on_disconnected = NULL;
	}
}


void 
execute_callback(redis_conn *c,reply_cb *stcb);


static void 
RecvFinish(redis_conn *c,int32_t bytestransfer,int32_t err_code)
{
	int32_t total_recv = 0;
	int32_t parse_ret;
	char   *ptr;
	do{	
		if(bytestransfer == 0 || (bytestransfer < 0 && err_code != EAGAIN)){
			_close(c,err_code);
			return;	
		}else if(bytestransfer > 0){
			c->recvbuf[bytestransfer] = 0;
			ptr = c->recvbuf;
			do{
				if(!c->tree) c->tree = parse_tree_new();
				parse_ret = parse(c->tree,&ptr);
				if(parse_ret == REDIS_OK){
					reply_cb *stcb = (reply_cb*)list_pop(&c->waitreplys);
					execute_callback(c,stcb);
					free(stcb);
					parse_tree_del(c->tree);
					c->tree = NULL;
					if(((socket_*)c)->status & SOCKET_CLOSE)
						return;
				}else if(parse_ret == REDIS_ERR){
					//error
					parse_tree_del(c->tree);
					c->tree = NULL;
					_close(c,ERDISPERROR);
					return;
				}else
					break;
			}while(1);
			if(total_recv >= RECV_BUFFSIZE){
				PostRecv(c);
				return;
			}else{
				bytestransfer = Recv(c);
				if(bytestransfer < 0 && (err_code = -bytestransfer) == EAGAIN) 
					return;
				else if(bytestransfer > 0)
					total_recv += bytestransfer;
			}
		}
	}while(1);
}

static void 
IoFinish(handle *sock,void *_,int32_t bytestransfer,
	     int32_t err_code)
{
	iorequest  *io = ((iorequest*)_);
	redis_conn *c  = (redis_conn*)sock;
	if(((socket_*)c)->status & SOCKET_CLOSE)
		return;
	if(io == (iorequest*)&c->send_overlap && bytestransfer > 0)
		SendFinish(c,bytestransfer);
	else if(io == (iorequest*)&c->recv_overlap)
		RecvFinish(c,bytestransfer,err_code);
}


int32_t 
_redis_execute(redis_conn *conn,const char *str)
{
	handle *h = (handle*)conn;
	int32_t ret = 0;
	if(((socket_*)h)->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	if(!h->e)
		return -ENOASSENG;
	packet *p = build_request(str);
	list_pushback(&conn->send_list,(listnode*)p);
	if(!(((socket_*)conn)->status & SENDING)){
		prepare_send(conn);
		ret = Send(conn,IO_NOW);
		if(ret > 0)
			update_send_list(conn,ret);
	}
	if(ret == -EAGAIN || ret > 0)
		return 0;
	return ret;	
}

int32_t 
redis_execute(redis_conn *conn,const char *str,
	        void (*cb)(redis_conn*,redisReply*,void *ud),
	        void *ud)
{

	int32_t ret = _redis_execute(conn,str);
	if(ret != 0) return ret;
	reply_cb *repobj = calloc(1,sizeof(*repobj));
	repobj->type = CB_C;
	if(cb){
		repobj->cb = cb;
		repobj->ud = ud;
	}
	list_pushback(&conn->waitreplys,(listnode*)repobj);
	return 0;
}

void 
redis_close(redis_conn *c)
{
	if(((socket_*)c)->status & SOCKET_RELEASE)
		return;
	close_socket((socket_*)c);
}

redis_conn*
redis_connect(engine *e,sockaddr_ *addr,
			  void (*on_disconnect)(redis_conn*,int32_t err))
{
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(fd < 0) return NULL;
	if(0 != easy_connect(fd,addr,NULL)){
		close(fd);
		return NULL;
	}
	redis_conn *conn = calloc(1,sizeof(*conn));
	((handle*)conn)->fd = fd;
	construct_stream_socket(&conn->base);
	((socket_*)conn)->dctor = redis_dctor;
	conn->on_disconnected = on_disconnect;	
	engine_associate(e,conn,IoFinish); 	
	PostRecv(conn);
	return conn;
}

typedef struct{
	engine *e;
    void (*connect_cb)(redis_conn*,int32_t,void*);
    void *ud;
    void (*on_disconnect)(redis_conn*,int32_t);
}stConnet;


static void on_connected(int32_t fd,int32_t err,void *ud){
	stConnet *st = (stConnet*)ud;
	if(fd >= 0 && err == 0){
		redis_conn *conn = calloc(1,sizeof(*conn));
		((handle*)conn)->fd = fd;
		construct_stream_socket(&conn->base);
		((socket_*)conn)->dctor = redis_dctor;	
		engine_associate(st->e,conn,IoFinish); 	
		PostRecv(conn);
		conn->on_disconnected = st->on_disconnect;
		st->connect_cb(conn,err,st->ud);
	}else if(err == ETIMEDOUT){
		st->connect_cb(NULL,err,st->ud);
	}
	free(st);
}

redis_conn*
redis_asyn_connect(engine *e,sockaddr_ *addr,
                   void (*connect_cb)(redis_conn*,int32_t,void*),
                   void *ud,
                   void (*on_disconnect)(redis_conn*,int32_t),
                   int32_t *err)
{
	if(err) *err = 0;
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(fd < 0){
		if(err) *err = errno; 
		return NULL;
	}
	easy_noblock(fd,1);
	int32_t ret;
	if(0 == (ret = easy_connect(fd,addr,NULL))){
		redis_conn *conn = calloc(1,sizeof(*conn));
		((handle*)conn)->fd = fd;
		construct_stream_socket(&conn->base);
		((socket_*)conn)->dctor = redis_dctor;	
		engine_associate(e,conn,IoFinish); 	
		PostRecv(conn);
		return conn;
	}
	else if(ret == -EINPROGRESS){
		stConnet *st = calloc(1,sizeof(*st));
		st->e = e;
		st->ud = ud;
		st->on_disconnect = on_disconnect;
		st->connect_cb = connect_cb;
		connector *contor = connector_new(fd,st,5000);
		engine_associate(e,contor,on_connected);			
	}else{
		if(err) *err = -ret;
		close(fd);
	}
	return NULL;		
}


#define LUAREDIS_METATABLE "luaredis_metatable"

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)


typedef struct{
	luaPushFunctor base;
	redis_conn    *c;
}stPushConn;

static void 
PushConn(lua_State *L,luaPushFunctor *_)
{
	stPushConn *self = (stPushConn*)_;
	lua_pushlightuserdata(L,self->c);
	luaL_getmetatable(L, LUAREDIS_METATABLE);
	lua_setmetatable(L, -2);		
}

static void 
build_resultset(redisReply* reply,lua_State *L){
	if(reply->type == REDIS_REPLY_INTEGER){
		lua_pushinteger(L,reply->integer);
	}else if(reply->type == REDIS_REPLY_STRING){
		lua_pushstring(L,reply->str);
	}else if(reply->type == REDIS_REPLY_ARRAY){
		lua_newtable(L);
		int i = 0;
		for(; i < reply->elements; ++i){
			build_resultset(reply->element[i],L);
			lua_rawseti(L,-2,i+1);
		}
	}else{
		lua_pushnil(L);
	}
}

typedef struct{
	luaPushFunctor base;
	redisReply    *reply;
}stPushResultSet;

static void PushResultSet(lua_State *L,luaPushFunctor *_){
	stPushResultSet *self = (stPushResultSet*)_;
	build_resultset(self->reply,L);
}

void 
execute_callback(redis_conn *c,reply_cb *stcb)
{
	if(stcb->type == CB_C && stcb->cb)
		stcb->cb(c,c->tree->reply,stcb->ud);
	else if(stcb->type == CB_LUA && stcb->luacb.L){
		//process lua callback
		const char *error;
		stPushConn st1;	
		st1.c = c;
		st1.base.Push = PushConn;
		redisReply *reply = c->tree->reply;
		if(reply->type == REDIS_REPLY_ERROR  || 
           reply->type == REDIS_REPLY_STATUS ||
           reply->type == REDIS_REPLY_STRING){
			if((error = LuaCallRefFunc(stcb->luacb,"fs",&st1,reply->str))){
				SYS_LOG(LOG_ERROR,"error on lua_redis_query_callback:%s\n",error);	
			}					
		}else if(reply->type == REDIS_REPLY_NIL){
			if((error = LuaCallRefFunc(stcb->luacb,"fp",&st1,NULL))){
				SYS_LOG(LOG_ERROR,"error on lua_redis_query_callback:%s\n",error);	
			}			
		}else if(reply->type == REDIS_REPLY_INTEGER){
			if((error = LuaCallRefFunc(stcb->luacb,"fi",&st1,reply->integer))){
				SYS_LOG(LOG_ERROR,"error on lua_redis_query_callback:%s\n",error);	
			}			
		}else{
			stPushResultSet st2;
			st2.base.Push = PushResultSet;
			st2.reply = reply;
			if((error = LuaCallRefFunc(stcb->luacb,"ff",&st1,&st2))){
				SYS_LOG(LOG_ERROR,"error on lua_redis_query_callback:%s\n",error);	
			}				
		}
		release_luaRef(&stcb->luacb);
	}
}

redis_conn*
lua_toreadisconn(lua_State *L, int index){
	return (redis_conn*)luaL_testudata(L, index, LUAREDIS_METATABLE);
}

void         	    
lua_on_disconnected(redis_conn *c,int32_t err)
{
	const char * error;
	stPushConn st;
	st.c = c;
	st.base.Push = PushConn;
	if((error = LuaCallRefFunc(c->lua_disconn_cb,"fi",&st,err))){
		SYS_LOG(LOG_ERROR,"error on redis lua_disconn_cb:%s\n",error);	
	}
}

int32_t
lua_redis_connect(lua_State *L)
{

	engine     *e  = lua_toengine(L,1);
	const char *ip = lua_tostring(L,2);
	uint16_t    port = lua_tointeger(L,3);
	sockaddr_   addr;
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(fd < 0){
		lua_pushstring(L,"open socket error\n");
		return 1;
	}
	easy_sockaddr_ip4(&addr,ip,port);
	if(0 != easy_connect(fd,&addr,NULL)){
		close(fd);
		lua_pushstring(L,"connect error\n");
		return 1;
	}
	redis_conn *conn = calloc(1,sizeof(*conn));
	((handle*)conn)->fd = fd;
	construct_stream_socket(&conn->base);
	((socket_*)conn)->dctor = redis_lua_dctor;

	if(LUA_TFUNCTION == lua_type(L,4)){
		conn->lua_disconn_cb  = toluaRef(L,4);
		conn->on_disconnected = lua_on_disconnected;
	}
	engine_associate(e,conn,IoFinish); 	
	PostRecv(conn);	
	lua_pushnil(L);
	lua_pushlightuserdata(L,conn);
	luaL_getmetatable(L, LUAREDIS_METATABLE);
	lua_setmetatable(L, -2);
	return 2;
}

int32_t 
lua_redis_close(lua_State *L)
{
	redis_conn *c = lua_toreadisconn(L,1);
	redis_close(c);
	return 0;
}

int32_t
lua_redis_execute(lua_State *L)
{
	redis_conn *c = lua_toreadisconn(L,1);
	const char *str = lua_tostring(L,2);
	int32_t ret = _redis_execute(c,str);
	if(ret != 0){
		lua_pushstring(L,"query error");
		return 1;
	}
	reply_cb *repobj = calloc(1,sizeof(*repobj));
	repobj->type = CB_LUA;
	if(LUA_TFUNCTION == lua_type(L,3))
		repobj->luacb = toluaRef(L,3);
	list_pushback(&c->waitreplys,(listnode*)repobj);
	lua_pushstring(L,"ok");
	return 1;
}


void
reg_luaredis(lua_State *L)
{
	luaL_Reg redis_mt[] = {
        {"__gc", lua_redis_close},
        {NULL, NULL}
    };

    luaL_Reg redis_methods[] = {
        {"Execute",  lua_redis_execute},
        {"Close",  lua_redis_close},
        {NULL,     NULL}
    };

    luaL_newmetatable(L, LUAREDIS_METATABLE);
    luaL_setfuncs(L, redis_mt, 0);

    luaL_newlib(L, redis_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    lua_newtable(L);
   	SET_FUNCTION(L,"Connect",lua_redis_connect);
   	//SET_FUNCTION(L,"AsynConnect",lua_redis_asynconnect);
}

//just for test

static parse_tree *test_tree = NULL;


static void show_reply(redisReply *reply){
	switch(reply->type){
		case REDIS_REPLY_STATUS:
		case REDIS_REPLY_ERROR:
		case REDIS_REPLY_STRING:
		{
			printf("%s\n",reply->str);
			break;
		}
		case REDIS_REPLY_INTEGER:
		{
			printf("%ld\n",reply->integer);
			break;
		}
		case REDIS_REPLY_ARRAY:{
			size_t i;
			for(i=0; i < reply->elements;++i){
				show_reply(reply->element[i]);
			}
			break;
		}
		case REDIS_REPLY_NIL:{
			printf("nil\n");
			break;
		} 
		default:{
			break;
		}
	}
}

void 
test_parse(char *str){
	int32_t parse_ret;
	do{
		if(!test_tree) test_tree = parse_tree_new();
		parse_ret = parse(test_tree,&str);
		if(parse_ret == REDIS_OK){
			show_reply(test_tree->reply);
			parse_tree_del(test_tree);
			test_tree = NULL;
		}else if(parse_ret == REDIS_ERR){
			//error
			parse_tree_del(test_tree);
			test_tree = NULL;
			printf("parse error\n");
			return;
		}else
			break;
	}while(1);
}