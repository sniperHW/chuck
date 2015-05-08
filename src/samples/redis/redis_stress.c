#include "chuck.h"
#include "db/redis/client.h"

//redis_conn *redis_client = NULL;
uint32_t    count = 0;


static void on_disconnect(redis_conn *conn,int32_t err)
{
	if(err != EACTCLOSE)
		redis_close(conn);
	printf("redis client on_disconnect\n");
}

void cmd_callback(redis_conn *conn,redisReply *reply,void *ud){
	++count;
	//char buff[1024];
	//snprintf(buff,1024,"hmget chaid:%d chainfo skills",(int)ud);
	redis_query(conn,"hmget chaid:1 chainfo skills",cmd_callback,ud);
}

int32_t timer_callback(uint32_t event,uint64_t _,void *ud){
	if(event == TEVENT_TIMEOUT){
		printf("count:%u/s\n",count);
		count = 0;
	}
	return 0;
}

int main(int argc,char **argv){

	if(argc < 2){
		printf("useage redis_streass set/get\n");
		exit(0);
	}

	int i;
	int testset = 0;
	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	easy_sockaddr_ip4(&server,"127.0.0.1",6379);
	redis_conn *redis_client = redis_connect(e,&server,on_disconnect);
	if(!redis_client){
		printf("connect to redis server %s:%u error\n","127.0.0.1",6379);
		return 0;
	}

	if(argc >= 2 && strcmp(argv[1],"set") == 0)
		testset = 1;
	for(i = 0; i < 1000; ++i){		
		char buff[1024];
		if(!testset){
			//snprintf(buff,1024,"hmget chaid:%d chainfo skills",i + 1);
			int tmp = i + 1;
			redis_query(redis_client,"hmget chaid:1 chainfo skills",cmd_callback,(void*)tmp);
		}else{
			snprintf(buff,1024,"hmset chaid:%d chainfo %s skills %s",
					 i + 1,"fasfsafasfsaf\rasfasfasdfsadfasdfasdfasfdfasdfasfdasdfasdf",
					 "fasdfasfasdfdsafdsafs\nadfsafa\r\nsdfsadfsadfasdfsadfsdafsdafsadfsdf" 
					);
			redis_query(redis_client,buff,NULL,NULL);
		}
	}
	engine_regtimer(e,1000,timer_callback,NULL);
	engine_run(e);

	return 0;
}