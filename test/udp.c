#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

double bytesize = 0;

double packet_count = 0;

uint64_t lastshow;

chk_bytechunk *chunk;

#define buffsize (1024 * 4)

const char buff[buffsize];

void server_event_cb(chk_datagram_socket *s,chk_datagram_event *event,int32_t error) {
	if(event) {
		bytesize += event->buff->datasize;
		packet_count += 1;
		uint64_t now = chk_systick();
		uint64_t duration = now - lastshow;
		if(duration >= 1000) {
			lastshow = now;
			printf("%.2fMB/s,%.2fpkt/s\n",(bytesize/1024/1024)*1000/duration,packet_count*1000/duration);
			bytesize = 0;
			packet_count = 0;			
		}
		chk_datagram_socket_sendto(s,chk_bytebuffer_clone(event->buff),&event->src);		
	}
}


void client_event_cb(chk_datagram_socket *s,chk_datagram_event *event,int32_t error) {
	if(event) {
		chk_datagram_socket_sendto(s,chk_bytebuffer_clone(event->buff),&event->src);		
	}
}

void start_server() {
    chk_sockaddr addr_local;
    int fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    easy_sockaddr_ip4(&addr_local,"127.0.0.1",8010);
    if(0 != easy_bind(fd,&addr_local)){
    	printf("server bind failed\n");
    }

    chk_datagram_socket *udpsocket = chk_datagram_socket_new(fd,SOCK_ADDR_IPV4);
    chk_loop_add_handle(loop,(chk_handle*)udpsocket,server_event_cb);

}

void start_client() {
	int fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	chk_datagram_socket *udpsocket = chk_datagram_socket_new(fd,SOCK_ADDR_IPV4);
	chk_loop_add_handle(loop,(chk_handle*)udpsocket,client_event_cb);
    chk_sockaddr addr_remote;
    easy_sockaddr_ip4(&addr_remote,"127.0.0.1",8010);
	chk_bytebuffer *msg = chk_bytebuffer_new_bychunk(chunk,0,chunk->cap);
	chk_datagram_socket_sendto(udpsocket,msg,&addr_remote);	
}

int main() {

	signal(SIGPIPE,SIG_IGN);
	loop = chk_loop_new();

	chunk = chk_bytechunk_new((void*)buff,sizeof(buff));	
	start_server();
	start_client();

	chk_loop_run(loop);	

	return 0;
}