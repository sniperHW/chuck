#include "chuck.h"
#include "session.h"


static void on_connected(int32_t fd,int32_t err,void *ud){
	if(fd >= 0 && err == 0){
		engine *e = (engine*)ud;
		handle *h = new_stream_socket(fd);
		engine_add(e,h,(generic_callback)transfer_finish);
		struct session *s = session_new(h);
		session_send(s,65535);
	}else if(err == ETIMEDOUT){
		printf("connect timeout\n");
	}
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]));
	uint32_t size = atoi(argv[3]);
	uint32_t i = 0;
	for( ; i < size; ++i){
		int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		easy_noblock(fd,1);
		int32_t ret;
		if(0 == (ret = easy_connect(fd,&server,NULL)))
			on_connected(fd,0,e);
		else if(ret == -EINPROGRESS){
			handle *contor = connector_new(fd,e,2000);
			engine_add(e,contor,(generic_callback)on_connected);			
		}else{
			close(fd);
			printf("connect to %s %d error\n",argv[1],atoi(argv[2]));
		}
	}
	engine_run(e);
	return 0;
}


