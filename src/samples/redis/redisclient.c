#include "chuck.h"
#include "db/redis/client.h"

redis_conn *redis_client = NULL;
int32_t     flag = 0;

static void on_disconnect(redis_conn *conn,int32_t err)
{
	redis_client = NULL;
	if(err != EACTCLOSE)
		redis_close(conn);
	printf("redis client on_disconnect\n");
}

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

void cmd_callback(redis_conn *conn,redisReply *reply,void *ud){
	printf("reply:\n");
	show_reply(reply);
	flag = 0;
}


int main(int argc,char **argv){
	test_parse_reply("*2\r");
	test_parse_reply("\n$53\r\nfasdfasdffasdfasdfasdfasdfasdfasfasdffasfsdffasdfadfs\r\n$36\r\nfasfsadfasdfsdafasdfasdfasdfsdafsadf\r\n");

	test_parse_reply("$5\r");
	test_parse_reply("\nhello\r");
	test_parse_reply("\n");

	test_parse_reply(":");
	test_parse_reply("10\r");
	test_parse_reply("\n");

	test_parse_reply("");
	test_parse_reply("+ok haha\r");
	test_parse_reply("\n");				

	char  input[65535];
	char *ptr;

	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	easy_sockaddr_ip4(&server,"127.0.0.1",6379);
	redis_client = redis_connect(e,&server,on_disconnect);
	if(!redis_client){
		printf("connect to redis server %s:%u error\n",argv[1],atoi(argv[2]));
		return 0;
	}

	redis_query(redis_client,"set kenny h\r\nha",cmd_callback,NULL);

	do{
	    ptr = input;	
	    while((*ptr = getchar()) != '\n')
	    	++ptr;
	    *ptr = 0;
	    flag = 1;
	    redis_query(redis_client,input,cmd_callback,NULL);
	    while(flag && redis_client){
	    	engine_runonce(e,10);
	    };
		if(!redis_client)
			return 0;
	}while(1);
	return 0;
}