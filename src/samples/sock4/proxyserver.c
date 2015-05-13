#include "chuck.h"
#include "packet/rawpacket.h"

enum{
	NONE,
	CONNECT,
	BIND,
	ESTABLISH,
	FAILED,
};

#pragma pack (1)
typedef struct{
	uint32_t VN:8;
	uint32_t CD:8;
	uint32_t DSTPORT:16;
	uint32_t DSTIP;
	char     USERID[8];
}request;
#pragma pack ()

#pragma pack (1)
typedef struct{
	uint32_t VN:8;
	uint32_t CD:8;
	uint32_t DSTPORT:16;
	uint32_t DSTIP;
}response;
#pragma pack ()

typedef struct{
	connection *inbound;
	connection *outbound;
	request     req;
	uint32_t    size;
	uint16_t    status;
}session;



void 
response_init(response *res,uint8_t CD,
			  uint16_t DSTPORT,uint32_t DSTIP)
{
	res->VN = 0;
	res->CD = CD;
	res->DSTPORT = DSTPORT;
	res->DSTIP = DSTIP;
}

static engine *e = NULL;

static uint32_t session_count = 0;

static void
close_peer(session *s,connection *c)
{
	if(c){
		connection_close(c);
		if(c == s->inbound){
			if(s->outbound){
				connection_close(s->outbound);
				s->outbound = NULL;
			}			
			s->inbound = NULL;
		}else{
			if(s->inbound){
				connection_close(s->inbound);
				s->inbound = NULL;
			}
			s->outbound = NULL;
		}
	}

	if(s->status != CONNECT && 
	  !s->inbound && !s->outbound)
	{
		free(s);
		session_count--;
	}	
}

static void 
outbound_packet(connection *c,packet *p,int32_t error)
{
	session *sess = (session*)c->ud_ptr;
	if(p){
		if(sess->status == ESTABLISH &&
		   sess->inbound){	
			connection_send(sess->inbound,make_writepacket(p),0);
			return;
		}
	}
	close_peer(sess,c);		
}

static void snd_fnish_cb(connection *c,packet *_)
{
	session *sess = (session*)c->ud_ptr;
	close_peer(sess,c);	
}

static void 
on_connected(int32_t fd,int32_t err,void *ud)
{
	session *sess = (session*)ud;
	response res;
	if(0 == err){
		if(sess->inbound){
			sess->outbound = connection_new(fd,4096,NULL);
			sess->outbound->ud_ptr = sess;
			sess->status = ESTABLISH;
			engine_associate(e,sess->outbound,outbound_packet);
			response_init(&res,90,0,0);
			rawpacket *pk = rawpacket_new(64);
			rawpacket_append(pk,&res,sizeof(res));
			connection_send(sess->inbound,(packet*)pk,0);
		}else{
			sess->status = FAILED;
			close(fd);
			close_peer(sess,NULL);					
		}
	}else{
		sess->status = FAILED;
		if(sess->inbound){
			response_init(&res,92,0,0);
			rawpacket *pk = rawpacket_new(64);
			rawpacket_append(pk,&res,sizeof(res));
			//notify on send finish
			connection_send(sess->inbound,(packet*)pk,snd_fnish_cb);
		}else{
			close_peer(sess,NULL);
		}	
	}
}

static int32_t 
process_request(session *sess)
{
	request *req = &sess->req;
	char ip[16];
	inet_ntop(AF_INET,&req->DSTIP,ip,16);	
	if(req->VN != 4)
		return -1;
	if(req->CD == 1){
		sockaddr_ addr;
		easy_sockaddr_ip4(&addr,ip,ntohs(req->DSTPORT));
		int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		int32_t ret;
		easy_noblock(fd,1);
		if(0 == (ret = easy_connect(fd,&addr,NULL))){
			on_connected(fd,0,sess);
		}else if(ret == -EINPROGRESS){
			connector *c = connector_new(fd,sess,2000);
			engine_associate(e,c,on_connected);
			sess->status = CONNECT; 			
		}else{
			close(fd);
			response res;
			response_init(&res,92,0,0);
			rawpacket *pk = rawpacket_new(64);
			rawpacket_append(pk,&res,sizeof(res));
			connection_send(sess->inbound,(packet*)pk,0);			
			return -1;
		}
	}else if(req->CD == 2){
		//don't support now
		return -1;
	}else
		return -1;
	return 0;
}


static void 
inbound_packet(connection *c,packet *p,int32_t error)
{
	session *sess = (session*)c->ud_ptr;
	if(p){
		if(sess->outbound && 
		   sess->status == ESTABLISH)
		{	
			connection_send(sess->outbound,make_writepacket(p),0);
			return;
		}
		else if(sess->status == NONE){
			//connect
			uint32_t cap = sizeof(sess->req) - sess->size;
			uint32_t size;
			void *ptr = rawpacket_data((rawpacket*)p,&size);
			if(size <= cap){
				memcpy((char*)(&sess->req) + sess->size,ptr,size);
				sess->size += size;
				if(sess->size > 8){
					uint32_t i;
					for(i = 8; i < sess->size; ++i){
						if(*((char*)&sess->req + i) == 0){
							//a request finish
							if(0 == process_request(sess))
								return;
							break;	
						}
					}
				}
			}
		}
	}
	close_peer(sess,c);
}


static void 
on_inbound_client(int32_t fd,sockaddr_ *_,void *ud)
{
	connection *c = connection_new(fd,4096,NULL);
	session *sess = calloc(1,sizeof(*sess));
	session_count++;
	sess->inbound = c;
	c->ud_ptr = sess;
	engine_associate(e,c,inbound_packet);
}

int32_t timer_callback(uint32_t event,uint64_t _,void *ud){
	if(event == TEVENT_TIMEOUT){
		printf("session:%u\n",session_count);
	}
	return 0;
}

int main(int argc,char **argv)
{
	signal(SIGPIPE,SIG_IGN);
	e = engine_new();
	sockaddr_ server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))){
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	easy_addr_reuse(fd,1);
	if(0 == easy_listen(fd,&server)){
		acceptor *a = acceptor_new(fd,NULL);
		engine_associate(e,a,on_inbound_client);
		engine_regtimer(e,1000,timer_callback,NULL);
		engine_run(e);
	}else{
		close(fd);
		printf("server start error\n");
	}
	return 0;
}



