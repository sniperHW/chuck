#include "chuck.h"
#include "packet/rawpacket.h"

uint32_t   packet_count = 0;

int32_t timer_callback(uint32_t event,uint64_t _,void *ud){
	if(event == TEVENT_TIMEOUT){
		printf("packet_count:%u/s\n",packet_count);
		packet_count = 0;
	}
	return 0;
}

static void on_packet(datagram *d,packet *p,sockaddr_ *from){
	datagram_send(d,make_writepacket(p),from);
	++packet_count;
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))){
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	easy_addr_reuse(fd,1);
	if(0 == easy_bind(fd,&server)){
		datagram *udpserver = datagram_new(fd,4096,NULL);
		engine_associate(e,udpserver,on_packet);
		engine_regtimer(e,1000,timer_callback,NULL);
		engine_run(e);
	}
	return 0;
}



