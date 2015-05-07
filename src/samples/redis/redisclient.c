#include "chuck.h"
#include "db/redis/client.h"


static void on_disconnect(redis_conn *conn,int32_t err)
{
	if(err != EACTCLOSE)
		redis_close(conn);
	printf("redis client on_disconnect\n");
}


void show_reply(redisReply *reply){
	switch(reply->type){
		case REDIS_STATUS:
		case REDIS_ERROR:
		case REDIS_BREPLY:
		{
			printf("%s\n",reply->str);
			break;
		}
		case REDIS_IREPLY:
		{
			printf("%ld\n",reply->integer);
			break;
		}
		case REDIS_MBREPLY:{
			size_t i;
			for(i=0; i < reply->elements;++i){
				show_reply(reply->element[i]);
			}
			break;
		}
		default:{
			break;
		}
	}
}

void cmd_callback(redisReply *reply,void *ud){
	show_reply(reply);
	exit(0);
}


int main(int argc,char **argv){

	if(argc < 2){
		printf("usage redisclient cmd\n");
		return 0;
	}

	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	easy_sockaddr_ip4(&server,"127.0.0.1",6379);
	redis_conn *conn = redis_connect(e,&server,on_disconnect);
	redis_query(conn,argv[1],cmd_callback,NULL);
	engine_run(e);
	return 0;
}