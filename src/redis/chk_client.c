#define _CORE_
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "util/chk_list.h"
#include "util/chk_error.h"
#include "util/chk_log.h"
#include "util/sds.h"
#include "util/chk_timer.h"
#include "util/chk_time.h"
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
typedef struct pending_reply pending_reply;

struct parse_tree {
	redisReply         *reply;
	parse_tree        **childs;
	size_t              want;
	size_t 				pos;
	char                type;
	char                break_;
	char   				tmp_buff[SIZE_TMP_BUFF]; 	 	
};

struct pending_reply {
	chk_list_entry entry;
	uint64_t deadline;
	void   (*cb)(chk_redisclient*,redisReply*,chk_ud ud);
	chk_ud ud;
};

struct chk_redisclient {
	int32_t                  status;
	chk_stream_socket       *sock;
    parse_tree              *tree;
    chk_event_loop          *loop;
    chk_ud                   ud;	
    chk_redis_connect_cb     cntcb;
    chk_redis_disconnect_cb  dcntcb;        //被动关闭时时回调
    chk_list                 waitreplys;
    chk_timer               *timer;
    int32_t                  timeout_count;
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
	char c,termi;
	redisReply *reply = current->reply;
	if(!reply->str) reply->str = current->tmp_buff;
	if(current->want) {
		//带了长度的string
		for(c=**str;*str != end && current->want; ++(*str),c=**str,--current->want)
			if(current->want > 2) reply->str[current->pos++] = c;//结尾的\r\n不需要
		if(current->want) return REDIS_RETRY;
	}else {
		for(;;) {
			termi = current->break_;	
			for(c=**str;*str != end && c != termi; ++(*str),c=**str)
				reply->str[current->pos++] = c;
			if(*str == end) return REDIS_RETRY;
			++(*str);
		    if(termi == '\n') break;
		    else current->break_ = '\n';
	    }
	    reply->len = current->pos;
	}
	assert(reply->len == current->pos);
	reply->str[current->pos] = 0;
	return REDIS_OK;
}

static int32_t parse_integer(parse_tree *current,char **str,char *end) {
	char c,termi;
	redisReply *reply = current->reply;	
	for(;;) {
		termi = current->break_;
		for(c=**str;*str != end && c != termi; ++(*str),c=**str)
			if(c == '-') current->want = -1;
			else if(!PARSE_NUM(integer)) {
				CHK_SYSLOG(LOG_ERROR,"PARSE_NUM failed");	
				return REDIS_ERR;
			}
		if(*str == end) return REDIS_RETRY;
		++(*str);					
	    if(termi == '\n') break;
	    else current->break_ = '\n';
    }
    reply->integer *= current->want;    
    return REDIS_OK;
}

static int32_t parse_breply(parse_tree *current,char **str,char *end) {
	char c,termi;
	redisReply *reply = current->reply;
	if(!current->want) {
		for(;;) {
			termi = current->break_;
			for(c=**str;*str != end && c != termi; ++(*str),c=**str)
				if(c == '-') reply->type = REDIS_REPLY_NIL;
				else if(!PARSE_NUM(len)) {
					CHK_SYSLOG(LOG_ERROR,"PARSE_NUM failed");		
					return REDIS_ERR;
				}
			if(*str == end) return REDIS_RETRY;
			++(*str);	
		    if(termi == '\n') {
		    	current->break_ = '\r';
		    	break;
		    }
		    else current->break_ = '\n';
	    };   
	    if(reply->type == REDIS_REPLY_NIL) return REDIS_OK;	    
	    current->want = reply->len + 2;//加上\r\n
	}

	if(!reply->str && reply->len + 1 > SIZE_TMP_BUFF) {
		reply->str = calloc(1,reply->len + 1);
		if(!reply->str) {
			CHK_SYSLOG(LOG_ERROR,"calloc parse_tree->str failed");	
			return REDIS_ERR;
		}
	}
	return parse_string(current,str,end);  
}


static parse_tree *parse_tree_new() {
	parse_tree *tree = calloc(1,sizeof(*tree));
	if(!tree) { 
		CHK_SYSLOG(LOG_ERROR,"calloc parse_tree failed");	
		return NULL;
	}
	tree->reply = calloc(1,sizeof(*tree->reply));
	if(!tree->reply) {
		CHK_SYSLOG(LOG_ERROR,"calloc parse_tree->reply failed");		
		free(tree);
		return NULL;
	}
	tree->break_ = '\r';
	return tree;
}

static void parse_tree_del(parse_tree *tree) {
	size_t i;
	if(tree->childs) {
		for(i = 0; i < tree->want; ++i) {
			parse_tree_del(tree->childs[i]);
		}
		free(tree->childs);
		free(tree->reply->element);
	}
	if(tree->reply->str != tree->tmp_buff) {
		free(tree->reply->str);
	}
	free(tree->reply);
	free(tree);
}

static int32_t parse(parse_tree *current,char **str,char *end);

static int32_t parse_mbreply(parse_tree *current,char **str,char *end) {
	size_t  i;
	int32_t ret,err;
	char    c,termi;
	redisReply *reply = current->reply;
	if(!current->want) {
		for(;;) {
			termi = current->break_;				
			for(c=**str;*str != end && c != termi; ++(*str),c=**str)
				if(c == '-') reply->type = REDIS_REPLY_NIL;
				else if(!PARSE_NUM(elements)){
					CHK_SYSLOG(LOG_ERROR,"PARSE_NUM failed");
					return REDIS_ERR;
				}
			if(*str == end) return REDIS_RETRY;
			++(*str);		    
		    if(termi == '\n'){
		    	current->break_ = '\r';
		    	break;
		    }
		    else current->break_ = '\n';
	    };	    
	    current->want = reply->elements;
	}

	if(current->want > 0 && !current->childs) {
		err = 0;
		do{
			current->childs = calloc(current->want,sizeof(*current->childs));
			if(!current->childs) {
				CHK_SYSLOG(LOG_ERROR,"calloc current->childs failed");	
				err = 1;
				break;
			}
			reply->element = calloc(current->want,sizeof(*reply->element));
			if(!reply->element) {
				CHK_SYSLOG(LOG_ERROR,"calloc current->element failed");	
				err = 1;
				break;
			}
			for(i = 0; i < current->want; ++i){
				current->childs[i] = parse_tree_new();
				if(!current->childs[i]) {
					CHK_SYSLOG(LOG_ERROR,"calloc current->childs[%d] failed",(int)i);	
					err = 1;
					break;
				}
				reply->element[i] = current->childs[i]->reply;
			}
		}while(0);

		if(err) {
			if(current->childs){
				for(i = 0; i < current->want; ++i) {
					if(current->childs[i])
						parse_tree_del(current->childs[i]);
				}
				free(current->childs);
			}
			if(reply->element)
				free(reply->element);
			current->childs = NULL;
			reply->element = NULL;
			return REDIS_ERR;
		}
	}

	for(;current->pos < current->want; ++current->pos) {
		if((*str) == end) return REDIS_RETRY;
		if(REDIS_OK != (ret = parse(current->childs[current->pos],str,end))) 
			return ret;
	}
	return REDIS_OK;	
}

#define IS_OP_CODE(CC)\
 (CC == '+'  || CC == '-'  || CC == ':'  || CC == '$'  || CC == '*')

static int32_t parse(parse_tree *current,char **str,char *end) {
	int32_t ret = REDIS_RETRY;
	redisReply *reply = current->reply;		
	if(!current->type) {
		char c = *(*str)++;
		if(IS_OP_CODE(c)) current->type = c;
		else {
			CHK_SYSLOG(LOG_ERROR,"invaild opcode:%d",(int)c);	
			return REDIS_ERR;
		}
	}
	switch(current->type) {
		case '+':{
			if(!reply->type) reply->type = REDIS_REPLY_STATUS;
			ret = parse_string(current,str,end);
			break;
		}
		case '-':{
			if(!reply->type) reply->type = REDIS_REPLY_ERROR;
			ret = parse_string(current,str,end);
			break;
		}
		case ':':{
			if(!reply->type) reply->type = REDIS_REPLY_INTEGER;
			current->want = 1;
			ret = parse_integer(current,str,end);
			break;
		}
		case '$':{
			if(!reply->type) reply->type = REDIS_REPLY_STRING;
			ret = parse_breply(current,str,end);
			break;
		}
		case '*':{
			if(!reply->type) reply->type = REDIS_REPLY_ARRAY;
			ret = parse_mbreply(current,str,end);
			break;
		}							
		default:{
			CHK_SYSLOG(LOG_ERROR,"invaild current->type:%d",(int)current->type);	
			return REDIS_ERR;
		}
	}		
	return ret;
}

static redisReply  connection_lose = {
	.type = REDIS_REPLY_ERROR,
	.str  = "client disconnected",
	.len  = 19,
};

static redisReply execute_timeout = {
	.type = REDIS_REPLY_ERROR,
	.str  = "timeout",
	.len  = 7,	
};   

static void destroy_redisclient(chk_redisclient *c,int32_t error) {
	pending_reply *stcb;
	if(c->tree) parse_tree_del(c->tree);
	while((stcb = cast(pending_reply*,chk_list_pop(&c->waitreplys)))) {
		if(stcb->cb) {
			stcb->cb(c,&connection_lose,stcb->ud);
		}
		free(stcb);
	}
	chk_stream_socket_close(c->sock,0);	
	if(c->timer) {
		chk_timer_unregister(c->timer);
	}
	
	if(c->dcntcb){
		c->dcntcb(c,c->ud,error);
	}

	free(c);
}

static void data_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	char *begin;
	uint32_t         datasize;
	uint32_t         size,pos;
	int32_t          parse_ret;
	pending_reply   *stcb;
	chk_bytechunk   *chunk;
	chk_redisclient *c = cast(chk_redisclient*,chk_stream_socket_getUd(s).v.val);
	if(data) { 
		datasize = data->datasize;
		chunk = data->head;
		for(pos = data->spos;datasize;) {
			begin = chunk->data + pos;
			size  = chunk->cap  - pos;
			size  = size > datasize ? datasize : size;
			if(!c->tree) c->tree = parse_tree_new();
			parse_ret = parse(c->tree,&begin,begin + size);
			if(REDIS_OK == parse_ret) {
				if(c->timeout_count > 0) {
					//忽略超时的返回
					--c->timeout_count;
				}else {
					stcb = cast(pending_reply*,chk_list_pop(&c->waitreplys));
					if(stcb->cb) {
						c->status |= CLIENT_INCB;					
						stcb->cb(c,c->tree->reply,stcb->ud);
						c->status ^= CLIENT_INCB;
					}	
					free(stcb);
				}
				parse_tree_del(c->tree);
				c->tree   = NULL;
				size      = (uint32_t)(begin - (chunk->data + pos));
				pos      += size;
				datasize -= size;
				if(c->status & CLIENT_CLOSE) {
					destroy_redisclient(c,0);
					return;
				}
				if(datasize && pos >= chunk->cap) {
					pos = 0;
					chunk = chunk->next;
				}
			}else if(parse_ret == REDIS_ERR){
				//parse出错,直接关闭连接
				c->status |= CLIENT_CLOSE;
				error = chk_error_redis_parse;
				/*if(c->dcntcb){
					c->dcntcb(c,c->ud,chk_error_redis_parse);
				}*/
				CHK_SYSLOG(LOG_ERROR,"redis reply parse error");	
				break;
			}else {
				pos       = 0;
				datasize -= size;
				chunk     = chunk->next;
			}
		}
	}
	else {
		c->status |= CLIENT_CLOSE;
		/*if(c->dcntcb){
			c->dcntcb(c,c->ud,error);
		}*/
	}

	if(c->status & CLIENT_CLOSE) {
		destroy_redisclient(c,error);
	}
}

static	const chk_stream_socket_option redis_socket_option = {
	.recv_buffer_size = 1024*16,
	.decoder = NULL,
};

static void connect_callback(int32_t fd,chk_ud ud,int32_t err) {
	chk_redisclient *c = cast(chk_redisclient*,ud.v.val);
	if(fd >= 0) {
		c->sock = chk_stream_socket_new(fd,&redis_socket_option);
		if(!c->sock) {
			CHK_SYSLOG(LOG_ERROR,"call chk_stream_socket_new failed");	
			c->cntcb(NULL,c->ud,chk_error_no_memory);
			free(c);
			return;
		}
		chk_stream_socket_setUd(c->sock,chk_ud_make_void(c));
		chk_loop_add_handle(c->loop,(chk_handle*)c->sock,data_cb);
		c->cntcb(c,c->ud,0);	
	} else {
		c->cntcb(NULL,c->ud,err);
		free(c);
	}
}

int32_t chk_redis_connect(chk_event_loop *loop,chk_sockaddr *addr,chk_redis_connect_cb cntcb,chk_ud ud) {
	chk_redisclient *c;
	if(!loop || !addr || !cntcb) {
		CHK_SYSLOG(LOG_ERROR,"invaild param loop:%p,addr:%p,cntcb:%p",loop,addr,cntcb);	
		return chk_error_invaild_argument;
	}
	c  = calloc(1,sizeof(*c));
	if(!c) {
		CHK_SYSLOG(LOG_ERROR,"calloc failed");	
		return chk_error_no_memory;
	}	
	c->cntcb  = cntcb;
	c->ud     = ud;
	c->loop   = loop;
    return chk_easy_async_connect(loop,addr,NULL,connect_callback,chk_ud_make_void(c),0);
}

void chk_redis_set_disconnect_cb(chk_redisclient *c,chk_redis_disconnect_cb cb,chk_ud ud) {
	c->dcntcb = cb;
	c->ud = ud;
}

void    chk_redis_close(chk_redisclient *c) {
	if(c->status & CLIENT_CLOSE) return;
	c->status |= CLIENT_CLOSE;
	if(!(c->status & CLIENT_INCB))
		destroy_redisclient(c,0);
}

/* Calculate the number of bytes needed to represent an integer as string. */
static inline int intlen(int64_t num) {
	if(num < 0)
	{
		return 1 + intlen(-num);
	}
	if(num < 10) return 1;
	else if(num < 100) return 2;
	else if(num < 1000) return 3;
	else if(num < 10000) return 4;
	else if(num < 100000) return 5;
	else if(num < 1000000) return 6;
	else if(num < 10000000) return 7;
	else if(num < 100000000) return 8;
	else if(num < 1000000000) return 9;
	else if(num < 10000000000) return 10;
	else if(num < 100000000000) return 11;		
	else if(num < 1000000000000) return 12;
	else if(num < 10000000000000) return 13;
	else if(num < 100000000000000) return 14;
	else if(num < 1000000000000000) return 15;		
	else if(num < 10000000000000000) return 16;
	else if(num < 100000000000000000) return 17;
	else if(num < 1000000000000000000) return 18;				
	else return 19;
}


/* Helper that calculates the bulk length given a certain string length. */
static inline size_t bulklen(size_t len) {
    return 1+intlen(len)+2+len+2;
}

static int redisvFormatCommand(char **target, const char *format, va_list ap) {
    const char *c = format;
    char *cmd = NULL; /* final command */
    int pos; /* position in final command */
    sds curarg, newarg; /* current argument */
    int touched = 0; /* was the current argument touched? */
    char **curargv = NULL, **newargv = NULL;
    int argc = 0;
    int totlen = 0;
    int j;

    /* Abort if there is not target to set */
    if (target == NULL){
		CHK_SYSLOG(LOG_ERROR,"target == NULL");	
        return -1;
    }

    /* Build the command string accordingly to protocol */
    curarg = sdsempty();
    if (curarg == NULL){
		CHK_SYSLOG(LOG_ERROR,"call sdsempty() failed");	
        return -1;
    }

    while(*c != '\0') {
        if (*c != '%' || c[1] == '\0') {
            if (*c == ' ') {
                if (touched) {
                    newargv = realloc(curargv,sizeof(char*)*(argc+1));
                    if (newargv == NULL){ 
                    	CHK_SYSLOG(LOG_ERROR,"call realloc() failed:%d",(int)(sizeof(char*)*(argc+1)));	
                    	goto err;
                    }
                    curargv = newargv;
                    curargv[argc++] = curarg;
                    totlen += bulklen(sdslen(curarg));

                    /* curarg is put in argv so it can be overwritten. */
                    curarg = sdsempty();
                    if (curarg == NULL) { 
                    	CHK_SYSLOG(LOG_ERROR,"call sdsempty() failed");	
                    	goto err;
                    }
                    touched = 0;
                }
            } else {
                newarg = sdscatlen(curarg,c,1);
                if (newarg == NULL){
                	CHK_SYSLOG(LOG_ERROR,"call sdscatlen() failed");	
                	goto err;
                }
                curarg = newarg;
                touched = 1;
            }
        } else {
            char *arg;
            size_t size;

            /* Set newarg so it can be checked even if it is not touched. */
            newarg = curarg;

            switch(c[1]) {
            case 's':
                arg = va_arg(ap,char*);
                size = strlen(arg);
                if (size > 0)
                    newarg = sdscatlen(curarg,arg,size);
                break;
            case 'b':
                arg = va_arg(ap,char*);
                size = va_arg(ap,size_t);
                if (size > 0)
                    newarg = sdscatlen(curarg,arg,size);
                break;
            case '%':
                newarg = sdscat(curarg,"%");
                break;
            default:
                /* Try to detect printf format */
                {
                    static const char intfmts[] = "diouxX";
                    char _format[16];
                    const char *_p = c+1;
                    size_t _l = 0;
                    va_list _cpy;

                    /* Flags */
                    if (*_p != '\0' && *_p == '#') _p++;
                    if (*_p != '\0' && *_p == '0') _p++;
                    if (*_p != '\0' && *_p == '-') _p++;
                    if (*_p != '\0' && *_p == ' ') _p++;
                    if (*_p != '\0' && *_p == '+') _p++;

                    /* Field width */
                    while (*_p != '\0' && isdigit(*_p)) _p++;

                    /* Precision */
                    if (*_p == '.') {
                        _p++;
                        while (*_p != '\0' && isdigit(*_p)) _p++;
                    }

                    /* Copy va_list before consuming with va_arg */
                    va_copy(_cpy,ap);

                    /* Integer conversion (without modifiers) */
                    if (strchr(intfmts,*_p) != NULL) {
                        va_arg(ap,int);
                        goto fmt_valid;
                    }

                    /* Double conversion (without modifiers) */
                    if (strchr("eEfFgGaA",*_p) != NULL) {
                        va_arg(ap,double);
                        goto fmt_valid;
                    }

                    /* Size: char */
                    if (_p[0] == 'h' && _p[1] == 'h') {
                        _p += 2;
                        if (*_p != '\0' && strchr(intfmts,*_p) != NULL) {
                            va_arg(ap,int); /* char gets promoted to int */
                            goto fmt_valid;
                        }
                        CHK_SYSLOG(LOG_ERROR,"invaild fmt");	
                        goto fmt_invalid;
                    }

                    /* Size: short */
                    if (_p[0] == 'h') {
                        _p += 1;
                        if (*_p != '\0' && strchr(intfmts,*_p) != NULL) {
                            va_arg(ap,int); /* short gets promoted to int */
                            goto fmt_valid;
                        }
                        CHK_SYSLOG(LOG_ERROR,"invaild fmt");	
                        goto fmt_invalid;
                    }

                    /* Size: long long */
                    if (_p[0] == 'l' && _p[1] == 'l') {
                        _p += 2;
                        if (*_p != '\0' && strchr(intfmts,*_p) != NULL) {
                            va_arg(ap,long long);
                            goto fmt_valid;
                        }
                        CHK_SYSLOG(LOG_ERROR,"invaild fmt");	
                        goto fmt_invalid;
                    }

                    /* Size: long */
                    if (_p[0] == 'l') {
                        _p += 1;
                        if (*_p != '\0' && strchr(intfmts,*_p) != NULL) {
                            va_arg(ap,long);
                            goto fmt_valid;
                        }
                        CHK_SYSLOG(LOG_ERROR,"invaild fmt");	
                        goto fmt_invalid;
                    }

                fmt_invalid:
                    va_end(_cpy);
                    goto err;

                fmt_valid:
                    _l = (_p+1)-c;
                    if (_l < sizeof(_format)-2) {
                        memcpy(_format,c,_l);
                        _format[_l] = '\0';
                        newarg = sdscatvprintf(curarg,_format,_cpy);

                        /* Update current position (note: outer blocks
                         * increment c twice so compensate here) */
                        c = _p-1;
                    }

                    va_end(_cpy);
                    break;
                }
            }

            if (newarg == NULL){
            	CHK_SYSLOG(LOG_ERROR,"newarg == NULL");	
            	goto err;
            }
            curarg = newarg;

            touched = 1;
            c++;
        }
        c++;
    }

    /* Add the last argument if needed */
    if (touched) {
        newargv = realloc(curargv,sizeof(char*)*(argc+1));
        if (newargv == NULL){
        	CHK_SYSLOG(LOG_ERROR,"realloc failed:%d",(int)(sizeof(char*)*(argc+1)));	
        	goto err;
        }
        curargv = newargv;
        curargv[argc++] = curarg;
        totlen += bulklen(sdslen(curarg));
    } else {
        sdsfree(curarg);
    }

    /* Clear curarg because it was put in curargv or was free'd. */
    curarg = NULL;

    /* Add bytes needed to hold multi bulk count */
    totlen += 1+intlen(argc)+2;

    /* Build the command at protocol level */
    cmd = malloc(totlen+1);
    if (cmd == NULL){
    	CHK_SYSLOG(LOG_ERROR,"calloc failed:%d",(int)(totlen+1));	
    	goto err;
    }

    pos = sprintf(cmd,"*%d\r\n",argc);
    for (j = 0; j < argc; j++) {
        pos += sprintf(cmd+pos,"$%zu\r\n",sdslen(curargv[j]));
        memcpy(cmd+pos,curargv[j],sdslen(curargv[j]));
        pos += sdslen(curargv[j]);
        sdsfree(curargv[j]);
        cmd[pos++] = '\r';
        cmd[pos++] = '\n';
    }
    assert(pos == totlen);
    cmd[pos] = '\0';

    free(curargv);
    *target = cmd;
    return totlen;

err:
    while(argc--)
        sdsfree(curargv[argc]);
    free(curargv);

    if (curarg != NULL)
        sdsfree(curarg);

    /* No need to check cmd since it is the last statement that can fail,
     * but do it anyway to be as defensive as possible. */
    if (cmd != NULL)
        free(cmd);

    return -1;
}


static inline int redisvAppendCommand(const char *format, va_list ap,chk_bytebuffer **bytebuffer) {
    char *cmd;
    int   len;

    len = redisvFormatCommand(&cmd,format,ap);
    if (len == -1) {
    	CHK_SYSLOG(LOG_ERROR,"call redisvFormatCommand failed");	
        return -1;
    }

    (*bytebuffer) = chk_bytebuffer_new(len);

    if(!(*bytebuffer)) {
    	CHK_SYSLOG(LOG_ERROR,"chk_bytebuffer_new failed:%d",len);	
    	free(cmd);
    	return -1;
    }

    if(chk_bytebuffer_append((*bytebuffer),(uint8_t*)cmd,(uint32_t)len) != chk_error_ok) {
    	CHK_SYSLOG(LOG_ERROR,"chk_bytebuffer_append failed");	
    	chk_bytebuffer_del((*bytebuffer));
    	(*bytebuffer) = NULL;
    	free(cmd);
    	return -1;    	
    }

    free(cmd);
    return 0;
}

typedef int32_t (*fn_socket_send)(chk_stream_socket *s,chk_bytebuffer *b);


static int32_t timeout_cb(uint64_t tick,chk_ud ud) {
	chk_redisclient *c = (chk_redisclient*)ud.v.val;
	uint64_t now = chk_systick();
	pending_reply  *repobj;
	if(!(c->status & CLIENT_INCB))
	while(NULL != (repobj = (pending_reply*)chk_list_begin(&c->waitreplys))) {
		if(now >= repobj->deadline) {
			chk_list_pop(&c->waitreplys);
			c->timeout_count++;//记录超时的数量，当收到返回时跳过对应的cb调用
			if(repobj->cb) {
				c->status |= CLIENT_INCB;					
				repobj->cb(c,&execute_timeout,repobj->ud);
				c->status ^= CLIENT_INCB;
				free(repobj);
				if(c->status & CLIENT_CLOSE) {
					destroy_redisclient(c,0);
					break;
				}
			}			
		} else {
			break;
		}
	}
	return 0; 
}

static int32_t _chk_redis_execute(fn_socket_send fn,chk_redisclient *c,chk_bytebuffer *buffer,chk_redis_reply_cb cb,chk_ud ud) {
	pending_reply  *repobj = NULL;
	int32_t         ret    = 0;
	repobj = calloc(1,sizeof(*repobj));
	if(!repobj){ 
    	CHK_SYSLOG(LOG_ERROR,"calloc repobj failed");	
		chk_bytebuffer_del(buffer);
		return chk_error_redis_request;
	}

	if(0 != (ret = fn(c->sock,buffer))) {
    	CHK_SYSLOG(LOG_ERROR,"chk_stream_socket_send failed:%d",ret);	
		free(repobj);
		return chk_error_redis_request;
	}

	repobj->cb = cb;
	repobj->ud = ud;
	repobj->deadline = chk_systick() + REDIS_DEFAULT_TIMEOUT * 1000;
	chk_list_pushback(&c->waitreplys,(chk_list_entry*)repobj);

	if(NULL == c->timer) {
		c->timer = chk_loop_addtimer(c->loop,1000,timeout_cb,chk_ud_make_void(c));
	}

	return 0;
}

int32_t chk_redis_execute(chk_redisclient *c,chk_redis_reply_cb cb,chk_ud ud,const char *fmt,...) {
	chk_bytebuffer *buffer = NULL;
	int32_t         ret    = 0;

    va_list ap;
    va_start(ap,fmt);
    ret = redisvAppendCommand(fmt,ap,&buffer);
    va_end(ap);

    if(0 != ret) {
    	CHK_SYSLOG(LOG_ERROR,"call redisvAppendCommand() failed");	
    	return chk_error_redis_request;
    }

	return _chk_redis_execute(chk_stream_socket_send,c,buffer,cb,ud);
}


#ifdef CHUCK_LUA

static int32_t append_head(chk_bytebuffer *buffer, uint32_t num) {
	char buff[64] = {0};
	int len = sprintf(buff,"*%u\r\n",num);
	return chk_bytebuffer_append(buffer,(uint8_t*)buff,(uint32_t)len);

}

static int32_t append_str(chk_bytebuffer *buffer,const char *str,size_t str_len) {
	char buff[64] = {0};
	int len = sprintf(buff,"$%u\r\n",(uint32_t)str_len);
	if(0 != chk_bytebuffer_append(buffer,(uint8_t*)buff,(uint32_t)len)) {
		return -1;
	}

	if(0 != chk_bytebuffer_append(buffer,(uint8_t*)str,(uint32_t)str_len)) {
		return -1;
	}

	if(0 != chk_bytebuffer_append(buffer,(uint8_t*)"\r\n",2)){
		return -1;
	}	
	return 0;
}


static int32_t append_int(chk_bytebuffer *buffer,int64_t num) {
	char buff[64] = {0};
	int  num_len  = intlen(num);	
	int  len = sprintf(buff,"$%d\r\n%lld\r\n",num_len,(long long int)num);
	return chk_bytebuffer_append(buffer,(uint8_t*)buff,(uint32_t)len);
}

static int32_t append_number(chk_bytebuffer *buffer,lua_State *L,int32_t index) {
	char buff[64] = {0};
	char num[64]  = {0};
	lua_Number v  = lua_tonumber(L,index);
	if(v != cast(lua_Integer,v)) {
		int num_len = sprintf(num,"%f",v);
		int len = sprintf(buff,"$%d\r\n%s\r\n",num_len,num);
		return chk_bytebuffer_append(buffer,(uint8_t*)buff,(uint32_t)len);
	} else {
		return append_int(buffer,cast(int64_t,v));
	}
}

static int32_t _chk_redis_execute_lua(fn_socket_send fn,chk_redisclient *c,const char *cmd,chk_redis_reply_cb cb,chk_ud ud,lua_State *L,int32_t start_idx,int32_t param_size)
{
	//pending_reply  *repobj  = NULL;
	chk_bytebuffer *buffer  = NULL;
	int32_t         i,idx,type;
	const char     *str;
	size_t          str_len;

	buffer = chk_bytebuffer_new(512);

	if(!buffer) {
    	CHK_SYSLOG(LOG_ERROR,"chk_bytebuffer_new(512) failed");	
		return chk_error_redis_request;
	}

	if(0 != append_head(buffer,param_size+1)) {
    	CHK_SYSLOG(LOG_ERROR,"append_head failed:%d",param_size+1);	
		chk_bytebuffer_del(buffer);
		return chk_error_redis_request;	
	}

	if(0 != append_str(buffer,cmd,strlen(cmd))) {
    	CHK_SYSLOG(LOG_ERROR,"append_str failed:%s",cmd);	
		chk_bytebuffer_del(buffer);
		return chk_error_redis_request;		
	}


	for(i = 0; i < param_size; ++i) {
		idx = start_idx + i;
		type = lua_type(L, idx);
		switch(type) {
			case LUA_TSTRING: {
				str = lua_tolstring(L,idx,&str_len);
				if(!str || 0 != append_str(buffer,str,(uint32_t)str_len)) {
					if(!str) {
						CHK_SYSLOG(LOG_ERROR,"str == NULL,param:%d",i);	
					}
					else {
						CHK_SYSLOG(LOG_ERROR,"append_str failed,param:%d",i);	
					}
					chk_bytebuffer_del(buffer);
					return chk_error_redis_request;		
				}
			}
			break;
			case LUA_TNUMBER: {
				if(0 != append_number(buffer,L,idx)) {
					CHK_SYSLOG(LOG_ERROR,"append_number failed,param:%d",i);	
					chk_bytebuffer_del(buffer);
					return chk_error_redis_request;			
				}
			}
			break;
			default: {
				chk_bytebuffer_del(buffer);
				return chk_error_redis_request;	
			}
			break;
		}
	}
	return _chk_redis_execute(fn,c,buffer,cb,ud);
}

int32_t chk_redis_execute_lua(chk_redisclient *c,const char *cmd,chk_redis_reply_cb cb,chk_ud ud,lua_State *L,int32_t start_idx,int32_t param_size) {
	return _chk_redis_execute_lua(chk_stream_socket_send,c,cmd,cb,ud,L,start_idx,param_size);
}


#endif
