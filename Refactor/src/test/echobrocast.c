#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

//一个解包器,包头2字节,表示后面数据大小.
typedef struct _decoder {
	void (*update)(chk_decoder*,chk_bytechunk *b,uint32_t spos,uint32_t size);
	chk_bytebuffer *(*unpack)(chk_decoder*,int32_t *err);
	void (*dctor)(chk_decoder*);
	uint32_t       spos;
	uint32_t       size;
	uint32_t       max;
	chk_bytechunk *b;
}_decoder;

static void _update(chk_decoder *_,chk_bytechunk *b,uint32_t spos,uint32_t size) {
	_decoder *d = ((_decoder*)_);
    if(!d->b) {
	    d->b = chk_bytechunk_retain(b);
	    d->spos  = spos;
	    d->size = 0;
    }
    d->size += size;
}

static chk_bytebuffer *_unpack(chk_decoder *_,int32_t *err) {
	_decoder *d = ((_decoder*)_);
	chk_bytebuffer *ret  = NULL;
	chk_bytechunk  *head = d->b;
	uint16_t        pk_len;
	uint32_t        pk_total,size,pos;
	do {
		if(d->size <= sizeof(pk_len)) break;
		size = sizeof(pk_len);
		pos  = d->spos;
		chk_bytechunk_read(head,(char*)&pk_len,&pos,&size);//读取payload大小
		pk_len = chk_ntoh16(pk_len);
		if(pk_len == 0) {
			if(err) *err = -1;
			break;
		}
		pk_total = size + pk_len;
		if(pk_total > d->max) {
			if(err) *err = CHK_EPKTOOLARGE;//数据包操作限制大小
			break;
		}
		if(pk_total > d->size) break;//没有足够的数据
		ret = chk_bytebuffer_new(head,d->spos,pk_total);
		//调整pos及其b
		do {
			head = d->b;
			size = head->cap - d->spos;
			size = pk_total > size ? size:pk_total;
			d->spos  += size;
			pk_total-= size;
			d->size -= size;
			if(d->spos >= head->cap) { //当前b数据已经用完
				d->b = chk_bytechunk_retain(head->next);
				chk_bytechunk_release(head);
				d->spos = 0;
				if(!d->b) break;
			}
		}while(pk_total);			
	}while(0);
	return ret;
}

static void _dctor(chk_decoder *_) {
	_decoder *d = ((_decoder*)_);
	if(d->b) chk_bytechunk_release(d->b);
}

static _decoder *_decoder_new(uint32_t max) {
	_decoder *d = calloc(1,sizeof(*d));
	d->update = _update;
	d->unpack = _unpack;
	d->max    = max;
	d->dctor  = _dctor;
	return d;
}


chk_stream_socket *clients[1000] ={0};
int        client_count = 0;
uint32_t   packet_count = 0;

void data_event_cb(chk_stream_socket *s,int32_t event,chk_bytebuffer *data) {
	int i;
	if(data) {
		//将数据包广播到所有连接
		for(i = 0;i < client_count; ++i){
			chk_stream_socket *_s = clients[i];
			if(_s){
				packet_count++;
				chk_stream_socket_send(_s,chk_bytebuffer_clone(data));
			}
		}		
	}else {
		for(i = 0;i < client_count; ++i) if(clients[i] == s) clients[i] = NULL;		
		--client_count;
		chk_stream_socket_close(s);
	}
}

void accept_cb(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	int i;
	chk_stream_socket_option option = {
		.recv_buffer_size = 65536,
		.recv_timeout = 0,
		.send_timeout = 0,
		.decoder = (chk_decoder*)_decoder_new(4096),
	};
	++client_count;		
	chk_stream_socket *s = chk_stream_socket_new(fd,&option);
	for(i = 0 ;i < 1000; ++i) {
		if(!clients[i]) {
			clients[i] = s;
			break;
		}
	}
	chk_loop_add_handle(loop,(chk_handle*)s,(chk_event_callback)data_event_cb);
}

int32_t on_timeout_cb(uint64_t tick,void*ud) {
	printf("client_count:%d,packet_count:%u/s\n",client_count,packet_count);
	packet_count = 0; 
	return 0; 
}

int main(char argc,char **argv) {
	loop = chk_loop_new();
	chk_sockaddr server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))) {
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	easy_addr_reuse(fd,1);
	if(0 == easy_listen(fd,&server)){
		chk_acceptor *a = chk_acceptor_new(fd,NULL);
		chk_loop_add_handle(loop,(chk_handle*)a,(chk_event_callback)accept_cb);
		chk_loop_addtimer(loop,1000,on_timeout_cb,NULL);
		chk_loop_run(loop);
	}else{
		close(fd);
		printf("server start error\n");
	}	
	return 0;
}

