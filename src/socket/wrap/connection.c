#include "socket/wrap/connection.h"
#include "engine/engine.h"
#include "packet/rawpacket.h"
#include "util/log.h"
#include "packet/luapacket.h"

static int32_t (*base_engine_add)(engine*,struct handle*,generic_callback) = NULL;

enum{
	RECVING   = SOCKET_END << 1,
	SENDING   = SOCKET_END << 2,
};

static inline void 
prepare_recv(connection *c)
{
	bytebuffer *buf;
	int32_t     i = 0;
	uint32_t    free_buffer_size,recv_size,pos;
	if(!c->next_recv_buf){
		c->next_recv_buf = bytebuffer_new(c->recv_bufsize);
		c->next_recv_pos = 0;
	}
	buf = c->next_recv_buf;
	pos = c->next_recv_pos;
	recv_size = c->recv_bufsize;
	do
	{
		free_buffer_size = buf->cap - pos;
		free_buffer_size = recv_size > free_buffer_size ? free_buffer_size:recv_size;
		c->wrecvbuf[i].iov_len = free_buffer_size;
		c->wrecvbuf[i].iov_base = buf->data + pos;
		recv_size -= free_buffer_size;
		pos += free_buffer_size;
		if(recv_size && pos >= buf->cap)
		{
			pos = 0;
			if(!buf->next)
				buf->next = bytebuffer_new(c->recv_bufsize);
			buf = buf->next;
		}
		++i;
	}while(recv_size);
	c->recv_overlap.iovec_count = i;
	c->recv_overlap.iovec = c->wrecvbuf;
}

static inline void 
PostRecv(connection *c)
{
	((socket_*)c)->status |= RECVING;
	prepare_recv(c);
	stream_socket_recv((stream_socket_*)c,&c->recv_overlap,IO_POST);		
}

static inline int32_t 
Recv(connection *c)
{
	prepare_recv(c);
	return stream_socket_recv((stream_socket_*)c,&c->recv_overlap,IO_NOW);		
}

static inline int32_t 
Send(connection *c,int32_t flag)
{
	int32_t ret = stream_socket_send((stream_socket_*)c,&c->send_overlap,flag);		
	if(ret < 0 && -ret == EAGAIN)
		((socket_*)c)->status |= SENDING;
	return ret; 
}


static inline void 
update_next_recv_pos(connection *c,
					 int32_t _bytestransfer)
{
	assert(_bytestransfer >= 0);
	uint32_t bytes = (uint32_t)_bytestransfer;
	uint32_t size;
	decoder_update(c->decoder_,c->next_recv_buf,c->next_recv_pos,bytes);
	do{
		size = c->next_recv_buf->cap - c->next_recv_pos;
		size = size > bytes ? bytes:size;
		c->next_recv_buf->size += size;
		c->next_recv_pos += size;
		bytes -= size;
		if(c->next_recv_pos >= c->next_recv_buf->cap)
		{
			bytebuffer_set(&c->next_recv_buf,c->next_recv_buf->next);
			c->next_recv_pos = 0;
		}
	}while(bytes);
}

static inline void 
_close(connection *c,int32_t err)
{
	((socket_*)c)->status |= SOCKET_CLOSE;
	engine_remove((handle*)c);
	if(c->on_disconnected){
		c->on_disconnected(c,err);
		c->on_disconnected = NULL;
	}
}

static void 
RecvFinish(connection *c,int32_t bytes,
		   int32_t err_code)
{
	int32_t total_recv = 0;
	packet *pk;	
	do{	
		if(bytes == 0 || (bytes < 0 && err_code != EAGAIN)){
			_close(c,err_code);
			return;	
		}else if(bytes > 0){
			update_next_recv_pos(c,bytes);
			int32_t unpack_err;
			do{
				pk = c->decoder_->unpack(c->decoder_,&unpack_err);
				if(pk){
					c->on_packet(c,pk,PKEV_RECV);
					packet_del(pk);
					if(((socket_*)c)->status & SOCKET_CLOSE)
						return;
				}else if(unpack_err != 0){
					_close(c,err_code);
					return;
				}
			}while(pk);
			if(total_recv >= c->recv_bufsize){
				PostRecv(c);
				return;
			}else{
				bytes = Recv(c);
				if(bytes < 0 && (err_code = -bytes) == EAGAIN) 
					return;
				else if(bytes > 0)
					total_recv += bytes;
			}
		}
	}while(1);
}

static inline iorequest*
prepare_send(connection *c)
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
update_send_list(connection *c,
				 int32_t _bytestransfer)
{
	assert(_bytestransfer >= 0);
	packet     *w;
	uint32_t    bytes = (uint32_t)_bytestransfer;
	uint32_t    size;
	do{
		w = (packet*)list_begin(&c->send_list);
		assert(w);
		if((uint32_t)bytes >= ((packet*)w)->len_packet)
		{
			if(w->mask){
				c->on_packet(c,w,PKEV_SEND);
				if(((socket_*)c)->status & SOCKET_CLOSE)
					return;
			}
			list_pop(&c->send_list);
			bytes -= ((packet*)w)->len_packet;
			packet_del(w);
		}else{
			do{
				size = ((packet*)w)->head->size - ((packet*)w)->spos;
				size = size > (uint32_t)bytes ? (uint32_t)bytes:size;
				bytes -= size;
				((packet*)w)->spos += size;
				((packet*)w)->len_packet -= size;
				if(((packet*)w)->spos >= ((packet*)w)->head->size)
				{
					((packet*)w)->spos = 0;
					((packet*)w)->head = ((packet*)w)->head->next;
				}
			}while(bytes);
		}
	}while(bytes);
}


static void 
SendFinish(connection *c,int32_t bytes)
{
	update_send_list(c,bytes);
	if(((socket_*)c)->status & SOCKET_CLOSE)
			return;
	if(!prepare_send(c)) {
		((socket_*)c)->status ^= SENDING;
		return;
	}
	Send(c,IO_POST);		
}

static void 
IoFinish(handle *sock,void *_,
		 int32_t bytes,int32_t err_code)
{
	iorequest  *io = ((iorequest*)_);
	connection *c  = (connection*)sock;
	if(((socket_*)c)->status & SOCKET_CLOSE)
		return;
	if(io == (iorequest*)&c->send_overlap && bytes > 0)
		SendFinish(c,bytes);
	else if(io == (iorequest*)&c->recv_overlap)
		RecvFinish(c,bytes,err_code);
}	

static int32_t 
imp_engine_add(engine *e,handle *h,
			   generic_callback callback)
{
	int32_t ret;
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	//call the base_engine_add first
	ret = base_engine_add(e,h,(generic_callback)IoFinish);
	if(ret == 0){
		((connection*)h)->on_packet = (void(*)(connection*,packet*,int32_t))callback;
		//post the first recv request
		if(!(((socket_*)h)->status & RECVING))
			PostRecv((connection*)h);
	}
	return ret;
}

int32_t 
connection_send(connection *c,packet *p,
				int32_t send_fsh_notify)
{
	int32_t ret;
	if(p->type != WPACKET && p->type != RAWPACKET){
		packet_del(p);
		return -EINVIPK;
	}	
	if(send_fsh_notify) p->mask = 1;	
	list_pushback(&c->send_list,(listnode*)p);
	if(!(((socket_*)c)->status & SENDING)){
		prepare_send(c);
		ret = Send(c,IO_NOW);
		if(ret < 0 && ret == -EAGAIN) 
			return -EAGAIN;
		else if(ret > 0)
			update_send_list(c,ret);
		return ret;
	}
	return -EAGAIN;
}

void 
connection_dctor(void *_)
{
	connection *c = (connection*)_;
	packet *p;
	if(c->on_disconnected)
		c->on_disconnected(c,0);	
	while((p = (packet*)list_pop(&c->send_list))!=NULL)
		packet_del(p);
	bytebuffer_set(&c->next_recv_buf,NULL);
	decoder_del(c->decoder_);
	free(c);
}

static void 
connection_init(connection *c,int32_t fd,
				uint32_t buffersize,decoder *d)
{
	((handle*)c)->fd = fd;
	c->recv_bufsize  = buffersize;
	c->next_recv_buf = bytebuffer_new(buffersize);
	construct_stream_socket(&c->base);
	//save socket_ imp_engine_add,and replace with self
	if(!base_engine_add)
		base_engine_add = ((handle*)c)->imp_engine_add; 
	((handle*)c)->imp_engine_add = imp_engine_add;
	c->decoder_ = d ? d:conn_raw_decoder_new();
	decoder_init(c->decoder_,c->next_recv_buf,0);
}

connection*
connection_new(int32_t fd,uint32_t buffersize,decoder *d)
{
	buffersize = size_of_pow2(buffersize);
    if(buffersize < MIN_RECV_BUFSIZE) buffersize = MIN_RECV_BUFSIZE;	
	connection *c 	 = calloc(1,sizeof(*c));
	connection_init(c,fd,buffersize,d);
	((socket_*)c)->dctor = connection_dctor;
	return c;
}

void 
connection_close(connection *c)
{
	if(((socket_*)c)->status & SOCKET_RELEASE)
		return;
	close_socket((socket_*)c);
}

static packet*
rawpk_unpack(decoder *d,int32_t *err)
{
	rawpacket  *raw;
	uint32_t    size;
	if(err) *err = 0;
	if(!d->size) return NULL;

	raw = rawpacket_new_by_buffer(d->buff,d->pos);
	size = d->buff->size - d->pos;
	d->pos  += size;
	d->size -= size;
	((packet*)raw)->len_packet = size;
	if(d->pos >= d->buff->cap){
		d->pos = 0;
		bytebuffer_set(&d->buff,d->buff->next);
	}
	return (packet*)raw;
}

decoder*
conn_raw_decoder_new()
{
	decoder *d = calloc(1,sizeof(*d));
	d->unpack = rawpk_unpack;
	return d;	
}


//for lua

#define LUA_METATABLE "conn_mata"


void 
lua_connection_dctor(void *_)
{
	connection *c = (connection*)_;
	packet *p;
	while((p = (packet*)list_pop(&c->send_list))!=NULL)
		packet_del(p);
	bytebuffer_set(&c->next_recv_buf,NULL);
	decoder_del(c->decoder_);
	release_luaRef(&c->lua_cb_packet);
	release_luaRef(&c->lua_cb_disconnected);
	//should not invoke free
}

connection*
lua_toconnection(lua_State *L, int index)
{
    return (connection*)luaL_testudata(L, index, LUA_METATABLE);
}

static int32_t 
lua_connection_new(lua_State *L)
{
	int32_t  fd;
	int32_t  buffersize;
	decoder *d = NULL;
	if(LUA_TNUMBER != lua_type(L,1))
		return luaL_error(L,"arg1 should be number");
	if(LUA_TNUMBER != lua_type(L,2))
		return luaL_error(L,"arg2 should be number");	
	

	fd = (int32_t)lua_tonumber(L,1);
	buffersize = (int32_t)lua_tonumber(L,2);
	if(LUA_TLIGHTUSERDATA == lua_type(L,3))
		d = (decoder*)lua_touserdata(L,3);

	connection *c = (connection*)lua_newuserdata(L, sizeof(*c));
	memset(c,0,sizeof(*c));
	connection_init(c,fd,buffersize,d);
	((socket_*)c)->dctor = lua_connection_dctor;
	luaL_getmetatable(L, LUA_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

typedef struct{
	luaPushFunctor base;
	connection    *c;
}stPushConn;

typedef struct{
	luaPushFunctor base;
	packet        *p;
}stPushPk;

static void 
PushConn(lua_State *L,luaPushFunctor *_)
{
	stPushConn *self = (stPushConn*)_;
	lua_pushlightuserdata(L,self->c);
	luaL_getmetatable(L, LUA_METATABLE);
	lua_setmetatable(L, -2);		
}

static void 
PushPk(lua_State *L,luaPushFunctor *_)
{
	stPushPk *self = (stPushPk*)_;
	lua_pushpacket(L,self->p);
}

static void 
lua_on_packet(connection *c,packet *p,int32_t event)
{
	const char * error;
	stPushConn st1;	
	if(c->lua_cb_packet.rindex == LUA_REFNIL)
		return;
	st1.c = c;
	st1.base.Push = PushConn;
	stPushPk st2;
	st2.p = clone_packet(p);
	st2.base.Push = PushPk;	
	if((error = LuaCallRefFunc(c->lua_cb_packet,"ffs",
							   &st1,&st2,
							   event == PKEV_RECV ? "RECV":"SEND"))){
		SYS_LOG(LOG_ERROR,"error on lua_cb_packet:%s\n",error);	
	}	
}

static void 
lua_on_disconnected(connection *c,int32_t err)
{
	const char * error;
	stPushConn st;
	st.c = c;
	st.base.Push = PushConn;
	if((error = LuaCallRefFunc(c->lua_cb_disconnected,"fi",&st,err))){
		SYS_LOG(LOG_ERROR,"error on lua_cb_disconnected:%s\n",error);	
	}
}


static int32_t 
lua_engine_add(lua_State *L)
{
	connection *c = lua_toconnection(L,1);
	engine     *e = lua_toengine(L,2);
	if(c && e){
		if(0 == imp_engine_add(e,(handle*)c,(generic_callback)lua_on_packet)){
			c->lua_cb_packet = toluaRef(L,3);
		}
	}
	return 0;
}

static int32_t 
lua_engine_remove(lua_State *L)
{
	connection *c = lua_toconnection(L,1);
	if(c){
		engine_remove((handle*)c);
	}
	return 0;
}


static int32_t 
lua_conn_close(lua_State *L)
{
	connection *c = lua_toconnection(L,1);
	connection_close(c);
	return 0;
}

static int32_t 
lua_conn_send(lua_State *L)
{
	connection *c = lua_toconnection(L,1);
	luapacket  *p = lua_topacket(L,2);
	int32_t     set_notify = lua_gettop(L) == 3;
	lua_pushinteger(L,connection_send(c,p->_packet,set_notify));
	p->_packet = NULL;
	return 1;
}

static int32_t 
lua_set_disconn_callback(lua_State *L)
{
	connection *c = lua_toconnection(L,1);
	if(LUA_TFUNCTION != lua_type(L,2))
		return luaL_error(L,"arg2 should be function");
	c->lua_cb_disconnected = toluaRef(L,2);
	connection_set_discntcb(c,lua_on_disconnected);
	return 0;
}

static int32_t 
lua_connection_gc(lua_State *L)
{
	connection *c = lua_toconnection(L,1);
	connection_close(c);
	return 0;
}

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)

void    
reg_luaconnection(lua_State *L)
{
    luaL_Reg conn_mt[] = {
        {"__gc", lua_connection_gc},
        {NULL, NULL}
    };

    luaL_Reg conn_methods[] = {
        {"Send",    lua_conn_send},
        {"Close",   lua_conn_close},
        {"Add2Engine",lua_engine_add},
        {"RemoveEngine",lua_engine_remove},
        {"SetDisConCb",lua_set_disconn_callback},
        {NULL,     NULL}
    };

    luaL_newmetatable(L, LUA_METATABLE);
    luaL_setfuncs(L, conn_mt, 0);

    luaL_newlib(L, conn_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

   	SET_FUNCTION(L,"connection",lua_connection_new);
}