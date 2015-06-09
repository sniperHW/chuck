#include "chuck.h"
#include "session.h"

sockaddr_ server;
engine *e;
uint32_t  total;
uint32_t  s1   = 0;
uint32_t  s2   = 0;

const char *ip;
uint16_t    port;


static void on_connected(int32_t fd,int32_t err,void *ud){
	if(fd >= 0 && err == 0){
		engine *e = (engine*)ud;
		stream_socket_ *h = new_stream_socket(fd);
		engine_associate(e,h,transfer_finish);
		struct session *s = session_new(h);
		session_recv(s,IO_POST);
		session_send(s,1024,IO_POST);
	}else if(err == ETIMEDOUT){
		printf("connect timeout\n");
	}
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	e = engine_new();
	ip = argv[1];
	port = atoi(argv[2]);
	easy_sockaddr_ip4(&server,ip,port);
	uint32_t n = atoi(argv[3]);
	uint32_t i = 0;
	for( ; i < n; ++i){
		int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		easy_noblock(fd,1);
		int32_t ret;
		if(0 == (ret = easy_connect(fd,&server,NULL)))
			on_connected(fd,0,e);
		else if(ret == -EINPROGRESS){
			connector *c = connector_new(fd,e,60000);
			engine_associate(e,c,on_connected);			
		}else{
			close(fd);
			printf("connect to %s %d error\n",ip,port);
		}
	}
	engine_run(e);
	return 0;
}


