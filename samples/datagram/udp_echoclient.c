#include "engine/engine.h"
#include "socket/socket_helper.h"


typedef struct {
	iorequest base;
	struct iovec wbuf[1];
	char   	  buf[4096];	
}udp_request;

udp_request *new_request(){
	udp_request *req = calloc(1,sizeof(*req));
	req->wbuf[0].iov_base = req->buf;
	req->wbuf[0].iov_len = 4096;
	req->base.iovec_count = 1;
	req->base.iovec = req->wbuf;
	return req;
}

static void 
datagram_callback(dgram_socket_ *h,void *_,int32_t bytes,
				  int32_t err,int32_t recvflags){
	udp_request *req = (udp_request*)_;
	if(err == ESOCKCLOSE){
		if(req) free(req);
		return;
	}
	if(req){
		datagram_socket_send(h,(iorequest*)req);
		datagram_socket_recv(h,(iorequest*)req,IO_POST,NULL);
	}
}

int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))){
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	dgram_socket_ *udpclient = new_datagram_socket(fd); 
	engine_associate(e,udpclient,datagram_callback);
	iorequest *req = (iorequest*)new_request();
	memcpy(&req->addr,&server,sizeof(server));
	datagram_socket_send(udpclient,req);
	datagram_socket_recv(udpclient,req,IO_POST,NULL);
	engine_run(e);
	return 0;
}