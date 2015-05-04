#include "chuck.h"
#include "packet/rawpacket.h"

static void on_packet(datagram *d,packet *p,sockaddr_ *from){
	//rawpacket *raw = (rawpacket*)p;
	//printf("%s\n",(const char *)rawpacket_data(raw,NULL));
	int32_t ret = datagram_send(d,make_writepacket(p),from);
	//printf("%d\n",ret);
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))){
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	datagram *udpclient = datagram_new(fd,4096,NULL); 
	engine_add(e,(handle*)udpclient,(generic_callback)on_packet);
	rawpacket *raw = rawpacket_new(512);
	rawpacket_append(raw,"hello world",strlen("hello world") + 1);
	datagram_send(udpclient,(packet*)raw,&server);
	engine_run(e);
	return 0;
}