#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;
uint32_t   packet_count = 0;

void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	chk_timer *t;
	if(!data) {	
		chk_stream_socket_close(s,0);
	}else{
		++packet_count;
		chk_bytebuffer *buffer = chk_bytebuffer_new(64);
		uint32_t len = 64 - sizeof(len);
		char data[64-sizeof(len)];
		memset(data,0,sizeof(data));
		len = chk_hton32(len);
		chk_bytebuffer_append(buffer,(uint8_t*)&len,sizeof(len));
		chk_bytebuffer_append(buffer,(uint8_t*)data,64-sizeof(len));	
		if(0 != chk_stream_socket_send(s,buffer,NULL,NULL))
		{
			printf("send error\n");
		}

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
    	if (0 == chk_connect(fd,&server,NULL,loop,NULL,NULL,-1))
    	{
    		chk_stream_socket_option option = {
				.recv_buffer_size = 1024*64,
				.decoder = (chk_decoder*)packet_decoder_new(4096),
			};
	
			chk_stream_socket *s = chk_stream_socket_new(fd,&option);
			chk_loop_add_handle(loop,(chk_handle*)s,data_event_cb);

			chk_bytebuffer *buffer = chk_bytebuffer_new(64);
			uint32_t len = 64 - sizeof(len);
			char data[64-sizeof(len)];
			memset(data,0,sizeof(data));
			len = chk_hton32(len);
			chk_bytebuffer_append(buffer,(uint8_t*)&len,sizeof(len));
			chk_bytebuffer_append(buffer,(uint8_t*)data,64-sizeof(len));	

			if(0 != chk_stream_socket_send(s,buffer,NULL,NULL))
			{
				printf("send error\n");
			}

    	}
	}
	chk_loop_addtimer(loop,1000,on_timeout_cb1,NULL);
	chk_loop_run(loop);
	return 0;
}

