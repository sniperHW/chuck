#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

int client_count = 0;

int cc = 0;

double bytesize = 0;

double packet_count = 0;

uint64_t lastshow;

chk_bytechunk *chunk;

#define buffsize (1024 * 4)

const char buff[buffsize];

const char *ip;
uint16_t    port;

typedef struct {
	chk_dlist_entry      entry;
	chk_stream_socket   *s;
}_client;

typedef struct {
	int32_t len;
	int32_t idx;
	char buf[64-8];
}packet;

chk_dlist _clients;

void server_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	if(data) {
		chk_dlist_entry *it = chk_dlist_begin(&_clients);
		chk_dlist_entry *end = chk_dlist_end(&_clients);
		for( ; it != end; it = it->next) {
			packet_count += 1;
			chk_stream_socket *ss = ((_client*)it)->s;
			chk_stream_socket_send(ss,chk_bytebuffer_clone(data));
		}
		uint64_t now = chk_systick();
		uint64_t duration = now - lastshow;
		if(duration >= 1000) {
			lastshow = now;
			printf("client:%d,pkt:%.2f/s\n",cc,(packet_count*1000)/duration);
			packet_count = 0;			
		}
	} else {	
		--cc;	
		chk_stream_socket_close(s,0);
		chk_dlist_entry *_c = (chk_dlist_entry*)chk_stream_socket_getUd(s).v.val;
		chk_dlist_remove(_c);
		free(_c);
	}
}

void on_new_client(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,chk_ud ud,int32_t err) {

	chk_stream_socket_option option = {
		.recv_buffer_size = buffsize,
		.decoder = (chk_decoder*)packet_decoder_new(buffsize),
	};

	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	_client *c = calloc(1,sizeof(*c));
	c->s = s;
	chk_stream_socket_setUd(s,chk_ud_make_void(s));
	chk_dlist_pushback(&_clients,(&c->entry));
	chk_loop_add_handle(loop,(chk_handle*)s,server_event_cb);
	++cc;
}

int server() {
	chk_dlist_init(&_clients);
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
		packet pk;
		chk_bytebuffer_read(data,(char*)&pk,sizeof(pk));
		if(pk.idx == chk_ntoh32(chk_stream_socket_getfd(s))) {
			chk_stream_socket_send(s,chk_bytebuffer_clone(data));
		}
	}
}

void connect_callback(int32_t fd,chk_ud ud,int32_t err) {
	if(0 == err) {
		chk_stream_socket_option option = {
			.recv_buffer_size = buffsize,
			.decoder = (chk_decoder*)packet_decoder_new(buffsize),
		};
		chk_stream_socket *s = chk_stream_socket_new(fd,&option);
		chk_loop_add_handle(loop,(chk_handle*)s,client_event_cb);
		packet pk;
		pk.len = chk_hton32(60);
		pk.idx = chk_hton32(fd);
		chk_bytebuffer *buffer = chk_bytebuffer_new(64);
		chk_bytebuffer_append(buffer,(uint8_t*)&pk,64);	
		chk_stream_socket_send(s,buffer);		
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
		printf("usage: benchmark_brocast [server|client|both] ip port [clientcount]\n");
		return 0;
	}

	const char *type = argv[1];

	if(strcmp(type,"client") == 0 || strcmp(type,"both") == 0 ) {
		if(argc < 5) {
			printf("usage: benchmark_brocast [server|client|both] ip port [clientcount]\n");
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