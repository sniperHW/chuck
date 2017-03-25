#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

uint32_t   packet_count = 0;

chk_stream_socket_option option = {
	.recv_buffer_size = 4096,
	.decoder = NULL,
};

struct client_connection {
	chk_stream_socket *s;
	chk_timer         *t;
	uint64_t           last_recv;
	uint64_t           recv_timeout;
};

void destroy_client_connection(struct client_connection *c);

int32_t timeout_cb(uint64_t tick,void *ud) {
	struct client_connection *c = (struct client_connection*)ud;
	if(c->last_recv + c->recv_timeout <= tick) {
		printf("recv timeout close connection\n");
		destroy_client_connection(c);
		return -1;
	}
	else {
		return 0;
	}
}

int32_t new_client_connection(struct client_connection **c,chk_stream_socket *s,uint64_t recv_timeout) {
	if(!c || !s) {
		return -1;
	}

	*c = calloc(1,sizeof(**c));

	if(!c) {
		return -1;
	}

	(*c)->s = s;

	if(recv_timeout > 0) {
		(*c)->recv_timeout = recv_timeout;
		(*c)->last_recv = chk_systick64();
		(*c)->t = chk_loop_addtimer(loop,100,timeout_cb,(void*)*c);
		if(!(*c)->t) {
			free(*c);
			(*c) = NULL;
			return -1;
		}
	}
	return 0;
}

void destroy_client_connection(struct client_connection *c) {
	if(c) {
		chk_timer_unregister(c->t);
		if(c->s) {
			chk_stream_socket_close(c->s,5000);
			c->s = NULL;
		}
		free(c);
	}
}


void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	
	struct client_connection *c = chk_stream_socket_getUd(s);

	if(data){

		++packet_count;

		c->last_recv = chk_systick64();
		/*if(0!= chk_bytebuffer_append(data,(uint8_t*)"hello",strlen("hello"))){
			printf("data is readonly\n");
		}*/
		chk_stream_socket_send(s,chk_bytebuffer_clone(data),NULL,NULL);
	}
	else{
		printf("socket close:%d\n",error);
		destroy_client_connection(c);
	}
	
}

void accept_cb(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	printf("accept_cb\n");
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	struct client_connection *c;
	if(0 != new_client_connection(&c,s,5000)) {
		chk_stream_socket_close(s,0);
	}
	else {
		chk_stream_socket_setUd(s,c);
		chk_loop_add_handle(loop,(chk_handle*)s,data_event_cb);
	}
}

static void signal_int(void *ud) {
	printf("signal_int\n");
	chk_loop_end(loop);
}

/*static void on_idle() {
	printf("idle\n");
}*/

int32_t on_timeout_cb1(uint64_t tick,void*ud) {
	printf("packet_count:%u/s\n",packet_count);
	packet_count = 0; 
	return 0; 
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
	else{
		CHK_SYSLOG(LOG_INFO,"server start");

		chk_loop_addtimer(loop,1000,on_timeout_cb1,NULL);

		//chk_loop_set_idle_func(loop,on_idle);

		chk_loop_run(loop);
	}
	chk_loop_del(loop);
	return 0;
}