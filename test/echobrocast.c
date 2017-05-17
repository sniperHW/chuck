#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

chk_stream_socket *clients[1000] ={0};
int        client_count = 0;
uint32_t   packet_count = 0;

void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	int i;
	if(data) {
		//将数据包广播到所有连接
		for(i = 0;i < client_count; ++i){
			chk_stream_socket *_s = clients[i];
			if(_s){
				packet_count++;
				chk_stream_socket_send(_s,chk_bytebuffer_clone(data),NULL,chk_ud_make_void(NULL));
			}
		}		
	}else {
		printf("socket close\n");
		for(i = 0;i < 1000; ++i){
			if(clients[i] == s) {
				--client_count;
				clients[i] = NULL;
				break;
			}
		}		
		chk_stream_socket_close(s,0);
	}
}

void accept_cb(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,chk_ud ud,int32_t err) {
	printf("accept_cb\n");
	int i;
	chk_stream_socket_option option = {
		.recv_buffer_size = 1024*16,
		.decoder = (chk_decoder*)packet_decoder_new(4096),
	};
	++client_count;		
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	for(i = 0 ;i < 1000; ++i) {
		if(!clients[i]) {
			clients[i] = s;
			break;
		}
	}
	chk_loop_add_handle(loop,(chk_handle*)s,data_event_cb);
}

int32_t on_timeout_cb(uint64_t tick,chk_ud ud) {
	uint32_t lasttick = chk_systick32();
	printf("client_count:%d,packet_count:%u,lasttick:%u\n",client_count,packet_count,lasttick);
	packet_count = 0; 
	return 0; 
}

int main(int argc,char **argv) {
	signal(SIGPIPE,SIG_IGN);
	loop = chk_loop_new();	
	if(!chk_listen_tcp_ip4(loop,argv[1],atoi(argv[2]),accept_cb,chk_ud_make_void(NULL)))
		printf("server start error\n");
	else {
		chk_loop_addtimer(loop,1000,on_timeout_cb,chk_ud_make_void(NULL));
		chk_loop_run(loop);
	}		
	return 0;
}

