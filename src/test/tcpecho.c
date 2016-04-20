#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

chk_stream_socket_option option = {
	.recv_buffer_size = 4096,
	.decoder = NULL,
};


void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	if(data){
		if(0!= chk_bytebuffer_append(data,(uint8_t*)"hello",strlen("hello"))){
			printf("data is readonly\n");
		}
		chk_stream_socket_send(s,chk_bytebuffer_clone(data));
	}
	else{
		printf("socket close:%d\n",error);
		chk_stream_socket_close(s,0);
	}
	
}

void accept_cb(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	printf("accept_cb\n");
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	chk_loop_add_handle(loop,(chk_handle*)s,data_event_cb);
}

static void signal_int(void *ud) {
	printf("signal_int\n");
	chk_loop_end(loop);
}

int main(int argc,char **argv) {
	signal(SIGPIPE,SIG_IGN);
	loop = chk_loop_new();

	if(0 != chk_watch_signal(loop,SIGINT,signal_int,NULL,NULL)) {
		printf("watch signal failed\n");
		return 0;
	}

	if(!chk_listen_tcp_ip4(loop,argv[1],atoi(argv[2]),accept_cb,NULL))
		printf("server start error\n");
	else
		chk_loop_run(loop);
	chk_loop_del(loop);
	return 0;
}