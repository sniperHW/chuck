#include "chuck.h"
#include "session.h"

int32_t timer_callback(uint32_t event,uint64_t _,void *ud)
{
	if(event == TEVENT_TIMEOUT){
		printf("client_count:%d,totalbytes:%f MB/s,packet_recv:%d\n",
				client_count,totalbytes/1024/1024,packet_recv);
		totalbytes  = 0.0;
		packet_recv = 0;
	}
	return 0;
}

static void on_connection(acceptor *a,int32_t fd,
						  sockaddr_ *_,void *ud,
						  int32_t err)
{
	if(err == EENGCLOSE){
		acceptor_del(a);
		return;
	}
	printf("on_connection\n");
	engine *e = (engine*)ud;
	stream_socket_ *h = new_stream_socket(fd);
	engine_associate(e,h,transfer_finish);
	struct session *s = session_new(h);
	session_recv(s,IO_POST);
	++client_count;
	printf("%d\n",client_count);	
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
		acceptor *a = acceptor_new(fd,e);
		engine_associate(e,a,on_connection);
		engine_regtimer(e,1000,timer_callback,NULL);
		engine_run(e);
	}else{
		close(fd);
		printf("server start error\n");
	}
	return 0;
}
