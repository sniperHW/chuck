#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

int client_count = 0;

uint32_t bytesize = 0;

chk_bytechunk *chunk;

const uint32_t buffsize = 1024 * 4;

char buff[buffsize];

const char *ip;
uint16_t    port;

chk_stream_socket_option option = {
	.recv_buffer_size = buffsize,
	.decoder = NULL,
}

void server_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	if(data) {
		bytesize += data->datasize;		
	} else {
		printf("client close\n");		
		chk_stream_socket_close(s,0);
	}
}

void on_new_client(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,chk_ud ud,int32_t err) {
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	chk_loop_add_handle(loop,(chk_handle*)s,server_event_cb);
}

int32_t on_server_timeout(uint64_t tick,chk_ud ud) {
	printf("%u\n",bytesize/1024/1024);
	bytesize = 0;
	return 0; 
}

int server() {
    chk_sockaddr addr_local;
    easy_sockaddr_ip4(&addr_local,ip,port);
	if(NULL != chk_listen(loop,&addr_local,on_new_client,chk_ud_make_void(NULL))){
		chk_loop_addtimer(loop,1000,on_server_timeout,chk_ud_make_void(NULL));
		return 0;
	} else {
		return -1;
	}
}


int32_t on_client_timeout(uint64_t tick,chk_ud ud) {
	chk_stream_socket *s = (chk_stream_socket*)ud.v.val;
	if(2 * buffsize  > chk_stream_socket_pending_send_size(s)) {
		chk_bytebuffer *msg = chk_bytebuffer_new_bychunk(chunk,0,chunk->cap);
		chk_stream_socket_send(s,msg,NULL,chk_ud_make_void(NULL));
	}
	return 0; 
}

void client_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	chk_timer *t;
	if(!data) {	
		t = (chk_timer*)chk_stream_socket_getUd(s).v.val;
		chk_timer_unregister(t);
		chk_stream_socket_close(s,0);
	}
}

void connect_callback(int32_t fd,chk_ud ud,int32_t err) {
	if(fd) {
		chk_stream_socket *s = chk_stream_socket_new(fd,&option);
		chk_timer *t = chk_loop_addtimer(loop,10,on_client_timeout,chk_ud_make_void(s));
		chk_stream_socket_setUd(s,chk_ud_make_void(t));
		chk_loop_add_handle(loop,(chk_handle*)s,client_event_cb);
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