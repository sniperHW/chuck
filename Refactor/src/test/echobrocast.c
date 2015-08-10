#include <stdio.h>
#include "chuck.h"
#include "testdecoder.h"

chk_event_loop *loop;


chk_stream_socket *clients[1000] ={0};
int        client_count = 0;
uint32_t   packet_count = 0;

void data_event_cb(chk_stream_socket *s,int32_t event,chk_bytebuffer *data) {
	int i;
	if(data) {
		//将数据包广播到所有连接
		for(i = 0;i < client_count; ++i){
			chk_stream_socket *_s = clients[i];
			if(_s){
				packet_count++;
				chk_stream_socket_send(_s,chk_bytebuffer_clone(data));
			}
		}		
	}else {
		for(i = 0;i < 1000; ++i){
			if(clients[i] == s) {
				--client_count;
				clients[i] = NULL;
				break;
			}
		}		
		chk_stream_socket_close(s);
	}
}

void accept_cb(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	int i;
	chk_stream_socket_option option = {
		.recv_buffer_size = 1024*16,
		.recv_timeout = 0,
		.send_timeout = 0,
		.decoder = (chk_decoder*)_decoder_new(4096),
	};
	++client_count;		
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	for(i = 0 ;i < 1000; ++i) {
		if(!clients[i]) {
			clients[i] = s;
			break;
		}
	}
	chk_loop_add_handle(loop,(chk_handle*)s,(chk_event_callback)data_event_cb);
}

int32_t on_timeout_cb(uint64_t tick,void*ud) {
	printf("client_count:%d,packet_count:%u/s,chunk_count:%u,buffercount:%u\n",
		    client_count,packet_count,chunkcount,buffercount);
	packet_count = 0; 
	return 0; 
}

int main(int argc,char **argv) {
	signal(SIGPIPE,SIG_IGN);
	loop = chk_loop_new();
	chk_sockaddr server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))) {
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	easy_addr_reuse(fd,1);
	if(0 == easy_listen(fd,&server)){
		chk_acceptor *a = chk_acceptor_new(fd,NULL);
		assert(0 == chk_loop_add_handle(loop,(chk_handle*)a,(chk_event_callback)accept_cb));
		chk_loop_addtimer(loop,1000,on_timeout_cb,NULL);
		chk_loop_run(loop);
	}else{
		close(fd);
		printf("server start error\n");
	}	
	return 0;
}

