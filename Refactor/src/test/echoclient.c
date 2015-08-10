#include <stdio.h>
#include "chuck.h"
#include "testdecoder.h"

chk_event_loop *loop;
uint32_t   packet_count = 0;

void data_event_cb(chk_stream_socket *s,int32_t event,chk_bytebuffer *data) {
	chk_timer *t;
	if(!data) {		
		t = (chk_timer*)chk_stream_socket_getUd(s);
		chk_timer_unregister(t);
		chk_stream_socket_close(s);
	}else{
		++packet_count;
	}
}

int32_t on_timeout_cb(uint64_t tick,void*ud) {
	chk_stream_socket *s = (chk_stream_socket*)ud;
	chk_bytebuffer *buffer = chk_bytebuffer_new(NULL,0,64);
	uint16_t len = 64 - sizeof(len);
	len = chk_hton16(len);
	uint32_t pos,size;
	pos  = 0;
	size = sizeof(len);
	chk_bytechunk_write(buffer->head,(char*)&len,&pos,&size);
	chk_stream_socket_send(s,buffer);
	return 0; 
}

void connect_callback(int32_t fd,int32_t err,void *ud) {
	chk_stream_socket_option option = {
		.recv_buffer_size = 1024*64,
		.recv_timeout = 0,
		.send_timeout = 0,
		.decoder = (chk_decoder*)_decoder_new(4096),
	};
	if(fd) {
		chk_stream_socket *s = chk_stream_socket_new(fd,&option);
		chk_timer *t = chk_loop_addtimer(loop,200,on_timeout_cb,s);
		chk_stream_socket_setUd(s,t);
		chk_loop_add_handle(loop,(chk_handle*)s,(chk_event_callback)data_event_cb);
	}
}

int32_t on_timeout_cb1(uint64_t tick,void*ud) {
	printf("packet_count:%u/s\n",packet_count);
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
	int32_t fd;
	int c = atoi(argv[3]);
	int i = 0;
	for(; i < c; ++i) {
    	fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    	chk_connect(fd,&server,NULL,loop,connect_callback,NULL,-1);
	}
	chk_loop_addtimer(loop,1000,on_timeout_cb1,NULL);
	chk_loop_run(loop);
	return 0;
}

