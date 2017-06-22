#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

int client_count = 0;

int c = 0;

double bytesize = 0;

double packet_count = 0;

uint64_t lastshow;

chk_bytechunk *chunk;

#define buffsize (1024 * 4)

const char buff[buffsize];

const char *ip;
uint16_t    port;

chk_stream_socket_option option = {
	.recv_buffer_size = buffsize,
	.decoder = NULL,
};

void server_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	if(data) {
		bytesize += data->datasize;
		packet_count += 1;
		uint64_t now = chk_systick();
		uint64_t duration = now - lastshow;
		if(duration >= 1000) {
			lastshow = now;
			printf("client:%d,%.2fMB/s,%.2fpkt/s\n",c,(bytesize/1024/1024)*1000/duration,packet_count*1000/duration);
			bytesize = 0;
			packet_count = 0;			
		}
		chk_stream_socket_send(s,chk_bytebuffer_clone(data),NULL,chk_ud_make_void(NULL));
	} else {	
		--c;	
		chk_stream_socket_close(s,0);
	}
}

void on_new_client(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,chk_ud ud,int32_t err) {
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	chk_loop_add_handle(loop,(chk_handle*)s,server_event_cb);
	++c;
}

int server() {
	lastshow = chk_systick();
    chk_sockaddr addr_local;
    easy_sockaddr_ip4(&addr_local,ip,port);
	if(NULL != chk_listen(loop,&addr_local,on_new_client,chk_ud_make_void(NULL))){
		return 0;
	} else {
		return -1;
	}
}

void client_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	if(!data) {	
		chk_stream_socket_close(s,0);
	}else {
		chk_stream_socket_send(s,chk_bytebuffer_clone(data),NULL,chk_ud_make_void(NULL));
	}
}

void connect_callback(int32_t fd,chk_ud ud,int32_t err) {
	if(0 == err) {
		chk_stream_socket *s = chk_stream_socket_new(fd,&option);
		chk_loop_add_handle(loop,(chk_handle*)s,client_event_cb);
		chk_bytebuffer *msg = chk_bytebuffer_new_bychunk(chunk,0,chunk->cap);
		chk_stream_socket_send(s,msg,NULL,chk_ud_make_void(NULL));		
	}else {
		printf("connect error\n");
	}
}

void client() {
	chk_sockaddr remote;
	if(0 != easy_sockaddr_ip4(&remote,ip,port)) {
		printf("invaild address:%s\n",ip);
		return;
	}

	chunk = chk_bytechunk_new((void*)buff,sizeof(buff));

	int i = 0;
	for(; i < client_count; ++i) {
    	chk_easy_async_connect(loop,&remote,NULL,connect_callback,chk_ud_make_void(NULL),-1);
	}
}

int main(int argc,char **argv) {

	if(argc < 4) {
		printf("usage: benchmark [server|client|both] ip port [clientcount]\n");
		return 0;
	}

	const char *type = argv[1];

	if(strcmp(type,"client") == 0 || strcmp(type,"both") == 0 ) {
		if(argc < 5) {
			printf("usage: benchmark [server|client|both] ip port [clientcount]\n");
			return 0;
		}
	}

	signal(SIGPIPE,SIG_IGN);
	loop = chk_loop_new();	

	ip = argv[2];
	port = atoi(argv[3]);

	if(strcmp(type,"server") == 0 || strcmp(type,"both") == 0) {
		if(0 != server()) {
			printf("server start failed %s:%u\n",ip,port);
			return 0;
		}
	}

	if(strcmp(type,"client") == 0 || strcmp(type,"both") == 0 ) {
		client_count = atoi(argv[4]);
		client();
	}

	chk_loop_run(loop);

	return 0;
}