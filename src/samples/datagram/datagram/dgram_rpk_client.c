#include "chuck.h"
#include "packet/wpacket.h"

static void on_packet(datagram *d,packet *p,sockaddr_ *from){
	datagram_send(d,make_writepacket(p),from);
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))){
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	datagram *udpclient = datagram_new(fd,4096,rpacket_decoder_new(4096)); 
	engine_associate(e,udpclient,on_packet);
	wpacket *w = wpacket_new(512);
	wpacket_write_uint32(w,100);
	wpacket_write_string(w,"hello world\n");
	datagram_send(udpclient,(packet*)w,&server);
	engine_run(e);
	return 0;
}