#include "engine/engine.h"
#include "socket/socket_helper.h"
#include "util/time.h"
#include "util/timerfd.h"

typedef struct {
	iorequest base;
	struct iovec wbuf[1];
	char   	  buf[4096];	
	int32_t   type;//0 for recv,1 for send
}udp_request;

udp_request *new_request(type){
	udp_request *req = calloc(1,sizeof(*req));
	req->wbuf[0].iov_base = req->buf;
	req->wbuf[0].iov_len = 4096;
	req->base.iovec_count = 1;
	req->base.iovec = req->wbuf;
	req->type = type;
	return req;
}

double totalbytes = 0.0;

static void timer_callback(void *ud){
	printf("totalbytes:%f MB/s\n",totalbytes/1024/1024);
	totalbytes = 0.0;
}

static void datagram_callback(handle *h,void *_,int32_t bytes,int32_t err,int32_t recvflags){
	udp_request *req = (udp_request*)_;
	if(err == ESOCKCLOSE){
		if(req) free(req);
		return;
	}
	if(req){
		if(bytes){
			if(req->type == 0){
				totalbytes += bytes;
				req->type = 1;
				datagram_socket_send(h,(iorequest*)req,IO_POST);
			}else{
				req->type = 0;
				datagram_socket_recv(h,(iorequest*)req,IO_POST,NULL);
			}
		}
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
	easy_addr_reuse(fd,1);
	if(0 == easy_bind(fd,&server)){
		handle *udpserver = new_datagram_socket(fd); 
		engine_add(e,udpserver,(generic_callback)datagram_callback);
		datagram_socket_recv(udpserver,(iorequest*)new_request(0),IO_POST,NULL);
		handle *tfd = timerfd_new(1000,NULL);
		engine_add(e,tfd,(generic_callback)timer_callback);
		engine_run(e);
	}
	return 0;
}