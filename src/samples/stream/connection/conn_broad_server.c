#include "chuck.h"
#include "packet/wpacket.h"
#include "packet/rpacket.h"


connection *clients[1000] ={0};

int        client_count = 0;
uint32_t   packet_count = 0;

static void timer_callback(void *ud){
	printf("client_count:%d,packet_count:%u/s\n",client_count,packet_count);
	packet_count = 0;
}

static void on_packet(connection *c,packet *p,int32_t event){
	if(event == PKEV_RECV){
		int i = 0;
		for(;i < client_count; ++i){
			connection *conn = clients[i];
			if(conn){
				packet_count++;
				connection_send(conn,make_writepacket(p),0);
			}
		}
	}
}

static void on_disconnected(connection *c,int32_t err){
	printf("on_disconnected %d\n",err);
	int i = 0;
	for(;i < client_count; ++i)
		if(clients[i]) clients[i] = NULL;	
	--client_count;
}

static void on_connection(int32_t fd,sockaddr_ *_,void *ud){
	printf("on_connection\n");
	engine *e = (engine*)ud;
	connection *c = connection_new(fd,65535,rpacket_decoder_new(1024));
	connection_set_discnt_callback(c,on_disconnected);
	engine_add(e,(handle*)c,(generic_callback)on_packet);

	int i = 0;
	for(i;i < 1000; ++i)
		if(!clients[i]){
			clients[i] = c;
			break;
		}
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
		engine_add(e,accptor,(generic_callback)on_connection);
		handle *tfd = timerfd_new(1000,NULL);
		engine_add(e,tfd,(generic_callback)timer_callback);
		engine_run(e);
	}else{
		close(fd);
		printf("server start error\n");
	}
	return 0;
}



