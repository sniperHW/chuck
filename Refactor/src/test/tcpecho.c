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
	if(data)
		chk_stream_socket_send(s,chk_bytebuffer_clone(data));
	else
		chk_stream_socket_close(s,0);
	
}

void accept_cb(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	printf("accept_cb\n");
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	chk_loop_add_handle(loop,(chk_handle*)s,(chk_event_callback)data_event_cb);
}

int main(int argc,char **argv) {
	signal(SIGPIPE,SIG_IGN);
	loop = chk_loop_new();
	if(!chk_listen_tcp_ip4(loop,argv[1],atoi(argv[2]),accept_cb,NULL))
		printf("server start error\n");
	return 0;
}