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
	if(!reply) {
		chk_redis_close(c);
		return;
	}
	++count;
	if(0 != chk_redis_execute(c,redis_reply_cb,NULL,"hmget %s %s %s","chaid:1","chainfo","skills")) {
		chk_redis_close(c);
		return;
	}
}

void redis_connect_cb(chk_redisclient *c,void *ud,int32_t err) {

	if(0 != err)
	{
		printf("connect to redis server failed\n");
		exit(0);
	}

	chk_redis_set_disconnect_cb(c,chk_redis_disconnect,NULL);	

	int i = 0;
	char buff[1024] = {0};
	printf("redis_connect_cb\n");
	
	for(i = 0; i < 1000; ++i){
		snprintf(buff,sizeof(buff) - 1,"chaid:%d",i);
		if(0 != chk_redis_execute(c,NULL,NULL,"hmget %s %s %s %s",buff,"chainfo","fasdfasfasdfasdfasdfasfasdfasfasdfdsaf",
						  "skills","fasfasdfasdfasfasdfasdfasdfcvavasdfasdf")) {
			chk_redis_close(c);
			return;
		}
	}

	printf("start get\n");


	for(i = 0; i < 1000; ++i) {
		if(0 != chk_redis_execute(c,redis_reply_cb,NULL,"hmget %s %s %s","chaid:1","chainfo","skills")) {
			chk_redis_close(c);
		}
		return;
	}	
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

