#define __CORE__
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "util/chk_list.h"
#include "util/chk_error.h"
#include "redis/chk_client.h"
#include "socket/chk_stream_socket.h"
#include "socket/chk_connector.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

#define SIZE_TMP_BUFF 512 //size must enough for status/error string

enum{
	CLIENT_CLOSE = 1 << 1,
	CLIENT_INCB  = 1 << 3,
};

typedef struct parse_tree parse_tree;

struct parse_tree {
	redisReply         *reply;
	parse_tree        **childs;
	size_t              want;
	size_t 				pos;
	char                type;
	char                break_;
	char   				tmp_buff[SIZE_TMP_BUFF]; 	 	
};

typedef struct {
	chk_list_entry entry;
	char          *buff;
	size_t         size;
}word;

typedef struct {
	chk_list_entry entry;
	void   (*cb)(chk_redisclient*,redisReply*,void *ud);
	void    *ud;
}pending_reply;

struct chk_redisclient {
	int32_t                  status;
	chk_stream_socket       *sock;
    parse_tree              *tree;
    chk_event_loop          *loop;
    void                    *ud;	
    chk_redis_connect_cb     cntcb;
    chk_redis_disconnect_cb  dcntcb;
    chk_list                 waitreplys;
};

#define IS_NUM(CC) (CC >= '0' && CC <= '9')

#define PARSE_NUM(FIELD)								  ({\
int ret = 0;												\
do{         												\
	if(IS_NUM(c)) {											\
		reply->FIELD = (reply->FIELD*10)+(c - '0');			\
		ret = 1;											\
	}														\
}while(0);ret;                                             })

static int32_t parse_string(parse_tree *current,char **str,char *end) {
	redisReply *reply = current->reply;
	char c;
	if(!reply->str)
		reply->str = current->tmp_buff;
	do {
		char termi = current->break_;	
		for(c=**str;*str != end && c != termi; ++(*str))
			reply->str[current->pos++] = c;
		if(*str == end) return REDIS_RETRY;
	    if(termi == '\n'){
	    	current->break_ = '\r';
	    	break;
	    }
	    else current->break_ = '\n';
    }while(1);
    if(current->want && current->pos != current->want)
    	return REDIS_ERR;	
	reply->str[current->pos] = 0;
	return REDIS_OK;
}

static int32_t parse_integer(parse_tree *current,char **str,char *end) {
	redisReply *reply = current->reply;	
	do{
		char c,termi = current->break_;
		for(c=**str;*str != end && c != termi; ++(*str)) {
			if(c == '1') current->want = -1;
			else if(!PARSE_NUM(integer)) return REDIS_ERR;
		}
		if(*str == end) return REDIS_RETRY;					
	    if(termi == '\n'){
	    	current->break_ = '\r';
	    	break;
	    }
	    else current->break_ = '\n';
    }while(1);

    reply->integer *= current->want;    
    return REDIS_OK;
}

static int32_t parse_breply(parse_tree *current,char **str,char *end) {
	redisReply *reply = current->reply;
	if(!current->want) {
		do{
			char c,termi = current->break_;
			for(c=**str;*str != end && c != termi; ++(*str)) {
				if(c == '-') reply->type = REDIS_REPLY_NIL;
				else if(!PARSE_NUM(len)) return REDIS_ERR;
			}
			if(*str == end) return REDIS_RETRY;	
		    if(termi == '\n'){
		    	current->break_ = '\r';
		    	break;
		    }
		    else current->break_ = '\n';
	    }while(1);   
	    if(reply->type == REDIS_REPLY_NIL)
	    	return REDIS_OK;	    
	    current->want = reply->len;
	}

	if(!reply->str) {
		if(reply->len + 1 > SIZE_TMP_BUFF)
			reply->str = calloc(1,reply->len + 1);
	}
	return parse_string(current,str,end);  
}


static parse_tree *parse_tree_new() {
	parse_tree *tree = calloc(1,sizeof(*tree));
	tree->reply = calloc(1,sizeof(*tree->reply));
	tree->break_ = '\r';
	return tree;
}

static void parse_tree_del(parse_tree *tree) {
	size_t i;
	if(tree->childs) {
		for(i = 0; i < tree->want; ++i)
			parse_tree_del(tree->childs[i]);
		free(tree->childs);
		free(tree->reply->element);
	}
	free(tree->reply);
	free(tree);
}

static int32_t parse(parse_tree *current,char **str,char *end);

static int32_t parse_mbreply(parse_tree *current,char **str,char *end) {
	redisReply *reply = current->reply;
	size_t  i;
	int32_t ret;
	if(!current->want) {
		do{
			char c,termi = current->break_;				
			for(c=**str;*str != end && c != termi; ++(*str))
				if(!PARSE_NUM(elements)) return REDIS_ERR;
			if(*str == end) return REDIS_RETRY;		    
		    if(termi == '\n'){
		    	current->break_ = '\r';
		    	break;
		    }
		    else current->break_ = '\n';
	    }while(1);	    
	    current->want = reply->elements;
	}

	if(!current->childs){
		current->childs = calloc(current->want,sizeof(*current->childs));
		reply->element = calloc(current->want,sizeof(*reply->element));
		for(i = 0; i < current->want; ++i){
			current->childs[i] = parse_tree_new();
			reply->element[i] = current->childs[i]->reply;
		}
	}

	for(;current->pos < current->want; ++current->pos) {
		if((*str) == end) 
			return REDIS_RETRY;
		ret = parse(current->childs[current->pos],str,end);
		if(ret != 0)
			return ret;
	}
	return REDIS_OK;	
}

#define IS_OP_CODE(CC)\
 (CC == '+'  || CC == '-'  || CC == ':'  || CC == '$'  || CC == '*')

static int32_t parse(parse_tree *current,char **str,char *end) {
	int32_t ret = REDIS_RETRY;
	redisReply *reply = current->reply;		
	while(!current->type) {
		char c = *(*str)++;
		if(IS_OP_CODE(c)){
			current->type = c;
			break;
		}
		else if(*str == end)
			return REDIS_RETRY;
		else
			return REDIS_ERR;
	}

	switch(current->type) {
		case '+':{
			reply->type = REDIS_REPLY_STATUS;
			ret = parse_string(current,str,end);
			break;
		}
		case '-':{
			reply->type = REDIS_REPLY_ERROR;
			ret = parse_string(current,str,end);
			break;
		}
		case ':':{
			reply->type = REDIS_REPLY_INTEGER;
			current->want = 1;
			ret = parse_integer(current,str,end);
			break;
		}
		case '$':{
			reply->type = REDIS_REPLY_STRING;
			ret = parse_breply(current,str,end);
			break;
		}
		case '*':{
			reply->type = REDIS_REPLY_ARRAY;
			ret = parse_mbreply(current,str,end);
			break;
		}							
		default:
			return REDIS_ERR;
	}		
	return ret;
}

static inline size_t digitcount(uint32_t num) {
	if(num < 10) return 1;
	else if(num < 100) return 2;
	else if(num < 1000) return 3;
	else if(num < 10000) return 4;
	else if(num < 100000) return 5;
	else if(num < 1000000) return 6;
	else if(num < 10000000) return 7;
	else if(num < 100000000) return 8;
	else if(num < 1000000000) return 9;
	else return 10;
}

static inline void u2s(uint32_t num,char **ptr) {
	char *tmp = *ptr + digitcount(num);
	do {
		*--tmp = '0'+(char)(num%10);
		(*ptr)++;
	}while(num/=10);	
}

//for request
static chk_bytebuffer *convert(chk_list *l,size_t space) {
	static char *end = "\r\n";
	chk_bytebuffer *buff;
	chk_bytechunk  *chunk;
	char *ptr,*ptr1;
	word *w;
	size_t i;		
	space += (digitcount(chk_list_size(l)) + 3);//plus head *,tail \r\n
	buff   =  chk_bytebuffer_new(NULL,0,space);
	chunk  =  buff->head;
	ptr1 = chunk->data;
	*ptr1++ = '*';
	u2s(chk_list_size(l),&ptr1);
	for(ptr=end;*ptr;)*ptr1++ = *ptr++;	
	while(NULL != (w = (word*)chk_list_pop(l))){
		*ptr1++ = '$';
		u2s((uint32_t)w->size,&ptr1);
		for(ptr=end;*ptr;)*ptr1++ = *ptr++;		
		for(i = 0; i < w->size;) *ptr1++ = w->buff[i++];
		for(ptr=end;*ptr;)*ptr1++ = *ptr++;	
		free(w);
	}
	return buff;
}


static chk_bytebuffer *build_request(const char *cmd) {
	chk_list l;
	size_t len   = strlen(cmd);
	word  *w = NULL;
	size_t i,j,space;
	char   quote  = 0;
	char   c;
	chk_list_init(&l);
	i = j = space = 0;
	for(; i < len; ++i) {
		c = cmd[i];
		if(c == '\"' || c == '\''){
			if(!quote)
				quote = c;
			else if(c == quote)
				quote = 0;
		}

		if(c != ' ') {
			if(!w){ 
				w = calloc(1,sizeof(*w));
				w->buff = (char*)&cmd[i];
			}
		}else if(!quote && w){
			//word finish
			w->size = &cmd[i] - w->buff;
			space += digitcount((uint32_t)w->size) + 3;//plus head $,tail \r\n
			space += w->size + 2;//plus tail \r\n
			chk_list_pushback(&l,(chk_list_entry*)w);	
			w = NULL;
			--i;
		}
	}
	if(w) {
		w->size = &cmd[i] - w->buff;
		space += digitcount((uint32_t)w->size) + 3;//plus head $,tail \r\n
		space += w->size + 2;//plus tail \r\n
		chk_list_pushback(&l,(chk_list_entry*)w);
	}
	return convert(&l,space);
}

static redisReply  dis_reply = {
	.type = REDIS_REPLY_ERROR,
	.str  = "client disconnected",
	.len  = 19,
};   

static void destroy_redisclient(chk_redisclient *c,int32_t err) {
	pending_reply *stcb;
	if(c->tree) parse_tree_del(c->tree);
	while((stcb = cast(pending_reply*,chk_list_pop(&c->waitreplys)))) {
		stcb->cb(c,&dis_reply,stcb->ud);
		free(stcb);
	}
	if(c->dcntcb) c->dcntcb(c,err);
	free(c);
}

static void data_cb(chk_stream_socket *s,int32_t event,chk_bytebuffer *data) {
	char *begin;
	uint32_t         datasize = data->datasize;
	uint32_t         size,pos;
	int32_t          parse_ret;
	pending_reply   *stcb;
	chk_bytechunk   *chunk    = data->head;
	chk_redisclient *c        = cast(chk_redisclient*,chk_stream_socket_getUd(s));
	if(data) for(pos = data->spos;datasize;) {
		begin = chunk->data + pos;
		size  = chunk->cap  - pos;
		size  = size > datasize ? datasize : size;
		if(!c->tree) c->tree = parse_tree_new();
		parse_ret = parse(c->tree,&begin,begin + size);
		if(parse_ret == REDIS_OK) {
			stcb = cast(pending_reply*,chk_list_pop(&c->waitreplys));
			if(stcb->cb) {
				c->status |= CLIENT_INCB;					
				stcb->cb(c,c->tree->reply,stcb->ud);
				c->status ^= CLIENT_INCB;
			}	
			free(stcb);
			parse_tree_del(c->tree);
			c->tree   = NULL;
			pos      += size;
			datasize -= size;
			if(c->status & CLIENT_CLOSE) {
				destroy_redisclient(c,0);
				return;
			}
			if(datasize) chunk = chunk->next;
		}else if(parse_ret == REDIS_ERR){
			event     = CHK_ERDSPASERR;
		}else
			break;
	}
	if(event != 0) chk_redis_close(c,event);
}

static	chk_stream_socket_option option = {
	.recv_buffer_size = 65536,
	.recv_timeout = 0,
	.send_timeout = 0,
	.decoder = NULL,
};

static void connect_callback(int32_t fd,int32_t err,void *ud) {
	chk_redisclient *c = cast(chk_redisclient*,ud);
	if(fd) {
		c->sock = chk_stream_socket_new(fd,&option);
		chk_stream_socket_setUd(c->sock,c);
		chk_loop_add_handle(c->loop,(chk_handle*)c->sock,(chk_event_callback)data_cb);
		c->cntcb(c,0,c->ud);	
	} else {
		c->cntcb(NULL,err,c->ud);
		free(c);
	}
}

int32_t chk_redis_connect(chk_event_loop *loop,chk_sockaddr *addr,
                          chk_redis_connect_cb cntcb,void *ud,
                          chk_redis_disconnect_cb dcntcb) {
	chk_redisclient *c;
	int32_t          fd;
	if(!loop || !addr || !cntcb) return -1;
	fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	c  = calloc(1,sizeof(*c));
	c->dcntcb = dcntcb;
	c->cntcb  = cntcb;
	c->ud     = ud;
	c->loop   = loop;
    return chk_connect(fd,addr,NULL,loop,connect_callback,c,-1);
}

void    chk_redis_close(chk_redisclient *c,int32_t err) {
	if(c->status & CLIENT_CLOSE) return;
	c->status |= CLIENT_CLOSE;
	chk_stream_socket_close(c->sock);
	if(!(c->status & CLIENT_INCB))
		destroy_redisclient(c,err);
}

int32_t chk_redis_execute(chk_redisclient *c,const char *str,chk_redis_reply_cb cb,void *ud) {
	pending_reply  *repobj;
	chk_bytebuffer *buffer;
	if(c->status & CLIENT_CLOSE) return -1;
	buffer = build_request(str);
	if(0 != chk_stream_socket_send(c->sock,buffer))
		return -1;
	repobj = calloc(1,sizeof(*repobj));
	if(cb) {
		repobj->cb = cb;
		repobj->ud = ud;
	}	
	chk_list_pushback(&c->waitreplys,(chk_list_entry*)repobj);
	return 0;
}

