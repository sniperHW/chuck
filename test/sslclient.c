#include <stdio.h>
#include "chuck.h"


chk_event_loop *loop;


void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	if(!data) {	
	}else{
		printf("recv response:%s\n",data->head->data);
	}
	chk_stream_socket_close(s,0);
}

void connect_callback(int32_t fd,chk_ud ud,int32_t err) {
	chk_stream_socket_option option = {
		.recv_buffer_size = 1024*64,
		.decoder = NULL,
	};

	if(fd) {
		chk_stream_socket *s = chk_stream_socket_new(fd,&option);

		if(0 == chk_ssl_connect(s)){
			chk_loop_add_handle(loop,(chk_handle*)s,data_event_cb);
			chk_bytebuffer *buffer = chk_bytebuffer_new(64);
			const char *msg = "world";
			chk_bytebuffer_append(buffer,(uint8_t*)msg,strlen(msg));	
			if(0 != chk_stream_socket_send(s,buffer)){
				printf("send error\n");
			}		
		}
		else {
			printf("ssl_connect error\n");
			chk_stream_socket_close(s,0);
		}		
	}
}

int main(int argc,char **argv) {
	signal(SIGPIPE,SIG_IGN);
	loop = chk_loop_new();
	chk_sockaddr server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))) {
		printf("invaild address:%s\n",argv[1]);
	}

    chk_easy_async_connect(loop,&server,NULL,connect_callback,chk_ud_make_void(NULL),-1);   	

	chk_loop_run(loop);
	return 0;
}

