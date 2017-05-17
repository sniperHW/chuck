#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;
uint32_t   packet_count = 0;

void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	chk_timer *t;
	if(!data) {	
		t = (chk_timer*)chk_stream_socket_getUd(s).v.val;
		chk_timer_unregister(t);
		chk_stream_socket_close(s,0);
	}else{
		++packet_count;
	}
}

int32_t on_timeout_cb(uint64_t tick,chk_ud ud) {
	chk_stream_socket *s = (chk_stream_socket*)ud.v.val;
	chk_bytebuffer *buffer = chk_bytebuffer_new(64);
	uint32_t len = 64 - sizeof(len);
	char data[64-sizeof(len)];
	memset(data,0,sizeof(data));
	len = chk_hton32(len);
	chk_bytebuffer_append(buffer,(uint8_t*)&len,sizeof(len));
	chk_bytebuffer_append(buffer,(uint8_t*)data,64-sizeof(len));	
	if(0 != chk_stream_socket_send(s,buffer,NULL,chk_ud_make_void(NULL)))
	{
		printf("send error\n");
	}
	return 0; 
}

void connect_callback(int32_t fd,chk_ud ud,int32_t err) {
	chk_stream_socket_option option = {
		.recv_buffer_size = 1024*64,
		.decoder = (chk_decoder*)packet_decoder_new(4096),
	};
	if(fd) {
		chk_stream_socket *s = chk_stream_socket_new(fd,&option);
		chk_timer *t = chk_loop_addtimer(loop,100,on_timeout_cb,chk_ud_make_void(s));
		chk_stream_socket_setUd(s,chk_ud_make_void(t));
		chk_loop_add_handle(loop,(chk_handle*)s,data_event_cb);
	}
}

int32_t on_timeout_cb1(uint64_t tick,chk_ud ud) {
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
    	chk_connect(fd,&server,NULL,loop,connect_callback,chk_ud_make_void(NULL),-1);
	}
	chk_loop_addtimer(loop,1000,on_timeout_cb1,chk_ud_make_void(NULL));
	chk_loop_run(loop);
	return 0;
}

