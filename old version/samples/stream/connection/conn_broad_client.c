#include "chuck.h"
#include "packet/wpacket.h"
#include "packet/rpacket.h"

uint32_t   packet_count = 0;

engine    *e;


int32_t timer_callback(uint32_t event,uint64_t _,void *ud){
	if(event == TEVENT_TIMEOUT){
		connection *c = (connection*)ud;
		packet *p = (packet*)wpacket_new(64);
		wpacket_write_uint64((wpacket*)p,(uint64_t)c);
		wpacket_write_string((wpacket*)p,"hello world\n");
		connection_send(c,p,NULL);
	}
	return 0;
}

static void on_packet(connection *c,packet *p,int32_t error){
	if(p){
		//rpacket *rpk = (rpacket*)p;
		//uint64_t id = rpacket_peek_uint64(rpk);
		//printf("%lld\n",id);
		//if(id == (uint64_t)c){
		//	packet_count++;
		//	connection_send(c,make_writepacket(p),NULL);
		//}
	}else{
		//error or peer close
		unregister_timer((timer*)c->ud_ptr);
		connection_close(c);
	}
}


static void on_connected(int32_t fd,int32_t err,void *ud){
	if(fd >= 0 && err == 0){
		engine *e = (engine*)ud;
		connection *c = connection_new(fd,65535,rpacket_decoder_new(1024));
		engine_associate(e,(handle*)c,on_packet);
		timer *t = engine_regtimer(e,20,timer_callback,c);
		c->ud_ptr = t;
		//packet *p = (packet*)wpacket_new(64);
		//wpacket_write_uint64((wpacket*)p,(uint64_t)c);
		//wpacket_write_string((wpacket*)p,"hello world\n");
		//connection_send(c,p,NULL);		
	}else{
		printf("connect error\n");
	}
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	e = engine_new();
	sockaddr_ server;
	easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]));
	uint32_t size = atoi(argv[3]);
	uint32_t i = 0;
	for( ; i < size; ++i){
		int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		easy_noblock(fd,1);
		int32_t ret;
		if(0 == (ret = easy_connect(fd,&server,NULL)))
			on_connected(fd,0,e);
		else if(ret == -EINPROGRESS){
			connector *c = connector_new(fd,e,2000);
			engine_associate(e,c,on_connected);			
		}else{
			close(fd);
			printf("connect to %s %d error\n",argv[1],atoi(argv[2]));
		}
	}
	//engine_regtimer(e,1000,timer_callback,NULL);
	engine_run(e);
	return 0;
}