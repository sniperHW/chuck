#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util/string.h"
#include "util/list.h"
#include "util/bytebuffer.h"
#include "packet/rawpacket.h"
#include "socket/socket_helper.h"
#include "db/redis/protocol.h"
#include "socket/wrap/wrap_comm.h"
#include "engine/engine.h"

enum{
	SENDING   = SOCKET_END << 2,
};

#define RECV_BUFFSIZE 1024*1

typedef struct{
	listnode node;
	void     (*cb)(redis_conn*,redisReply*,void *ud);
	void    *ud;
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
    void         	    (*on_disconnected)(struct redis_conn*,int32_t err);
}redis_conn;

void 
redis_dctor(void *_)
{
	redis_conn *c = (redis_conn*)_;
	packet *p;
	if(c->on_disconnected)
		c->on_disconnected(c,0);	
	while((p = (packet*)list_pop(&c->send_list))!=NULL)
		packet_del(p);
	if(c->tree)
		parse_tree_del(c->tree);
	free(c);
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
					if(stcb->cb)
						stcb->cb(c,c->tree->reply,stcb->ud);
					free(stcb);
					parse_tree_del(c->tree);
					c->tree = NULL;
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
redis_query(redis_conn *conn,const char *str,
	        void (*cb)(redis_conn*,redisReply*,void *ud),
	        void *ud)
{
	handle *h = (handle*)conn;
	int32_t ret = 0;
	if(((socket_*)h)->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	if(!h->e)
		return -ENOASSENG;
	packet *p = build_request(str);
	reply_cb *repobj = calloc(1,sizeof(*repobj));
	if(cb){
		repobj->cb = cb;
		repobj->ud = ud;
	}
	list_pushback(&conn->waitreplys,(listnode*)repobj);
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

void redis_close(redis_conn *c){
	if(((socket_*)c)->status & SOCKET_RELEASE)
		return;
	close_socket((socket_*)c);
}

redis_conn *redis_connect(engine *e,sockaddr_ *addr,void (*on_disconnect)(redis_conn*,int32_t err))
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
	engine_associate(e,conn,IoFinish); 	
	PostRecv(conn);
	return conn;
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
test_parse_reply(char *str){
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