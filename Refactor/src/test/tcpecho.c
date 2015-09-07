#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

chk_stream_socket_option option = {
	.recv_buffer_size = 4096,
	.recv_timeout = 0,
	.send_timeout = 0,
	.decoder = NULL,
};


void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	if(data) {
		chk_stream_socket_send(s,chk_bytebuffer_clone(data));
	}else {
		chk_stream_socket_close(s);
	}
}

void accept_cb(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	printf("accept_cb\n");
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	chk_loop_add_handle(loop,(chk_handle*)s,(chk_event_callback)data_event_cb);
}

int main(int argc,char **argv) {
	signal(SIGPIPE,SIG_IGN);
	chk_sockaddr server;
	loop = chk_loop_new();
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))) {
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	easy_addr_reuse(fd,1);
	if(0 == easy_listen(fd,&server)){
		chk_acceptor *a = chk_acceptor_new(fd,NULL);
		chk_loop_add_handle(loop,(chk_handle*)a,(chk_event_callback)accept_cb);
		chk_loop_run(loop);
	}else{
		close(fd);
		printf("server start error\n");
	}	
	return 0;
}