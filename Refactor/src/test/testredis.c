#include <stdio.h>
#include "chuck.h"


//chk_redisclient *client;
chk_event_loop  *loop;
int32_t count = 0;

void show_reply(redisReply *reply){
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

void chk_redis_disconnect(chk_redisclient *c,void *ud,int32_t err) {
	printf("chk_redis_disconnect:%d\n",err);
}

void redis_reply_cb(chk_redisclient *c,redisReply *reply,void *ud) {
	//show_reply(reply);
	++count;
	//if(count == 689){
	//	printf("fdasf");
	//}
	chk_redis_execute(c,"hmget chaid:1 chainfo skills",redis_reply_cb,NULL);
}

void redis_connect_cb(chk_redisclient *c,void *ud,int32_t err) {
	int i = 0;
	printf("redis_connect_cb\n");
	chk_redis_set_disconnect_cb(c,chk_redis_disconnect,NULL);
	for( ; i < 1000; ++i) chk_redis_execute(c,"hmget chaid:1 chainfo skills",redis_reply_cb,NULL);
}

int32_t on_timeout_cb(uint64_t tick,void*ud) {
	printf("count:%d/s\n",count);
	count = 0; 
	return 0; 
}

int main() {
	chk_sockaddr server;
	loop = chk_loop_new();
	if(0 != easy_sockaddr_ip4(&server,"127.0.0.1",6379)) {
		return 0;
	}		
	chk_redis_connect(loop,&server,redis_connect_cb,NULL);
	chk_loop_addtimer(loop,1000,on_timeout_cb,NULL);
	chk_loop_run(loop);
	return 0;
}

