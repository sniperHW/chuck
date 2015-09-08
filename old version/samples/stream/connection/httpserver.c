#include "chuck.h"
#include "packet/rawpacket.h"

const char *response = "HTTP/1.1 200 OK\r\n"\
					   "Connection: Keep-Alive\r\n"\
					   "Content-Type: text/plain\r\n"\
					   "Content-Length: 12\r\n\r\n"\
					   "hello world!";


engine  *e;

static void on_packet(connection *c,packet *p,int32_t error){
	if(p){
		rawpacket *wpk = rawpacket_new(strlen(response));
		rawpacket_append(wpk,cast(void*,response),strlen(response)); 
		connection_send(c,cast(packet*,wpk),NULL);
	}else{
		connection_close(c);
	}
}

static void on_connection(acceptor *a,int32_t fd,sockaddr_ *_,void *ud,int32_t err){
	if(err == EENGCLOSE){
		acceptor_del(a);
		return;
	}	
	engine *e = (engine*)ud;
	connection *c = connection_new(fd,65535,http_decoder_new(65535));
	engine_associate(e,c,on_packet);
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	e = engine_new();
	sockaddr_ server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))){
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	easy_addr_reuse(fd,1);
	if(0 == easy_listen(fd,&server)){
		acceptor *a = acceptor_new(fd,e);
		engine_associate(e,a,on_connection);
		if(EENGCLOSE != engine_run(e))
			engine_del(e);
	}else{
		close(fd);
		printf("server start error\n");
	}
	return 0;
}



