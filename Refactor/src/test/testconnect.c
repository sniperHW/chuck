#include <stdio.h>
#include "chuck.h"
int c = 0;

void connect_callback(int32_t fd,int32_t err,void *ud) {
	if(fd) {
		printf("%d\n", ++c);
	}
}

int main(char argc,char **argv) {
	int i,c;
	chk_sockaddr server;
	int32_t fd;	
	chk_event_loop *loop;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))) {
		printf("invaild address:%s\n",argv[1]);
		return 0;
	}
	c = atoi(argv[3]);
    loop = chk_loop_new();
    for(i = 0; i < c; ++i) {
    	fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    	chk_connect(fd,&server,NULL,loop,connect_callback,NULL,-1);
    }
    chk_loop_run(loop);
	return 0;
}