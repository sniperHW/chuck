#include "chuck.h"


int      client_count = 0;
double   totalbytes   = 0;

int32_t timer_callback(uint32_t event,uint64_t _,void *ud){
	if(event == TEVENT_TIMEOUT){
		printf("client_count:%d,totalbytes:%f MB/s\n",client_count,totalbytes/1024/1024);
		totalbytes = 0.0;
	}
	return 0;
}

static void on_packet(connection *c,packet *p,int32_t event){
	if(event == PKEV_RECV){
		printf("on_packet\n");
		connection_send(c,make_writepacket(p),1);
	}else{
		printf("packet send finish\n");
		connection_close(c);
	}
}

static void on_disconnected(connection *c,int32_t err){
	--client_count;
}

static void on_connection(int32_t fd,sockaddr_ *_,void *ud){
	printf("on_connection\n");
	engine *e = (engine*)ud;
	connection *c = connection_new(fd,64,NULL);
	connection_set_discnt_callback(c,on_disconnected);
	engine_associate(e,c,on_packet);
	++client_count;
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))){
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	easy_addr_reuse(fd,1);
	if(0 == easy_listen(fd,&server)){
		handle *accptor = acceptor_new(fd,e);
		engine_associate(e,accptor,on_connection);
		engine_regtimer(e,1000,timer_callback,NULL);
		engine_run(e);
	}else{
		close(fd);
		printf("server start error\n");
	}
	return 0;
}



