#include "chuck.h"
#include "packet/rawpacket.h"

uint32_t   packet_count = 0;

static void timer_callback(void *ud){
	printf("packet_count:%u/s\n",packet_count);
	packet_count = 0;
}

static void on_packet(datagram *d,packet *p,sockaddr_ *from){
	//f(5 == ++packet_count){
	//	printf("here\n");
	//}
	int32_t ret = datagram_send(d,make_writepacket(p),from);
	++packet_count;
	//printf("%d\n",ret);
	//rawpacket *raw = (rawpacket*)p;
	//printf("%s\n",(const char *)rawpacket_data(raw,NULL));
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
		engine_add(e,(handle*)udpserver,(generic_callback)on_packet);
		handle *tfd = timerfd_new(1000,NULL);
		engine_add(e,tfd,(generic_callback)timer_callback);
		engine_run(e);
	}
	return 0;
}



