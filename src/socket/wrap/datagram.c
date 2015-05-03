/*
static void datagram_destroy(void *ptr)
{
	datagram_t c = (datagram_t)ptr;
	buffer_release(c->recv_buf);
	kn_close_sock(c->handle);
	destroy_decoder(c->_decoder);
	if(c->ud && c->destroy_ud){
		c->destroy_ud(c->ud);
	}
	free(c);				
}

static int raw_unpack(decoder *_,void* _1){
	((void)_);
	datagram_t c = (datagram_t)_1;
	packet_t r = (packet_t)rawpacket_create1(c->recv_buf,0,c->recv_buf->size);
	c->on_packet(c,r,&c->recv_overlap.addr); 
	destroy_packet(r);
	return 0;
}

static int rpk_unpack(decoder *_,void *_1){
	((void)_);
	datagram_t c = (datagram_t)_1;
	if(c->recv_buf->size <= sizeof(uint32_t))
		return 0;	
	uint32_t pk_len = 0;
	uint32_t pk_hlen;
	buffer_read(c->recv_buf,0,(int8_t*)&pk_len,sizeof(pk_len));
	pk_hlen = kn_ntoh32(pk_len);
	uint32_t pk_total_size = pk_hlen+sizeof(pk_len);
	if(c->recv_buf->size < pk_total_size)
		return 0;	
	packet_t r = (packet_t)rpk_create(c->recv_buf,0,pk_len);
	c->on_packet(c,r,&c->recv_overlap.addr); 
	destroy_packet(r);	
	return 0;
}	
*/

#include "socket/wrap/datagram.h"
#include "engine/engine.h"

enum{
	RECVING   = 1 << 5,
};

static int32_t (*base_engine_add)(engine*,struct handle*,generic_callback) = NULL;

int32_t datagram_send(datagram *d,packet *p,sockaddr_ *addr)
{
	iorequest  *o;
	int32_t     ret,i;
	uint32_t    size,pos,buffer_size;
	bytebuffer *b;	
	do{	
		if(!addr){
			ret = -EINVAL;
			break;
		}
		if(p->type != WPACKET && p->type != RAWPACKET){
			ret = -EINVIPK;
			break;
		}
		i = 0;
		size = 0;
		pos = p->spos;
		b = p->head;
		buffer_size = p->len_packet;
		while(i < MAX_WBAF && b && buffer_size)
		{
			d->wsendbuf[i].iov_base = b->data + pos;
			size = b->size - pos;
			size = size > buffer_size ? buffer_size:size;
			buffer_size -= size;
			d->wsendbuf[i].iov_len = size;
			++i;
			b = b->next;
			pos = 0;
		}
		if(buffer_size != 0){
			ret = -EMSGSIZE;
			break;		
		}
		o.iovec_count = i;
		o.iovec = d->wsendbuf;
		o.addr = *addr;
		ret = datagram_socket_send((handle*)d,o,IO_NOW);
	}while(0);
	packet_del(p);
	return ret;
}

static inline void prepare_recv(datagram *d){
	if(d->recv_buf)
		buffer_release(d->recv_buf);
	c->recv_buf = buffer_create(c->recv_bufsize);
	c->wrecvbuf.iov_len = c->recv_bufsize;
	c->wrecvbuf.iov_base = c->recv_buf->data;
	c->recv_overlap.iovec_count = 1;
	c->recv_overlap.iovec = &c->wrecvbuf;	
}

static inline void Recv(datagram_t c){
	(((socket_*)h)->status |= RECVING;
	prepare_recv(c);
	kn_sock_post_recv(c->handle,&c->recv_overlap);	
	c->doing_recv = 1;	
}

static void IoFinish(handle *sock,void *_,int32_t bytestransfer,int32_t err_code)
{
	datagram *c = (datagram*)sock;	
	if(bytestransfer > 0 && ((iorequest)_)->recvflags != MSG_TRUNC){
		//c->recv_buf->size = bytestransfer;
		//c->_decoder->unpack(c->_decoder,c);
	}
	PostRecv(c);
}


static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback){
	int32_t ret;
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	//call the base_engine_add first
	ret = base_engine_add(e,h,(generic_callback)IoFinish);
	if(ret == 0){
		((datagram*)h)->on_packet = (void(*)(datagram*,packet*))callback;
		//post the first recv request
		if(!(((socket_*)h)->status & RECVING))
			Recv((datagram*)h);
	}
	return ret;
}

void datagram_dctor(void *_)
{
	datagram *d = (datagram*)_;
	bytebuffer_set(&d->next_recv_buf,NULL);
	decoder_del(d->decoder_);	
}

datagram *datagram_new(int32_t fd,uint32_t buffersize,decoder *d)
{
	buffersize = size_of_pow2(buffersize);
    if(buffersize < MIN_RECV_BUFSIZE) buffersize = MIN_RECV_BUFSIZE;	
	datagram *dgarm 	 = calloc(1,sizeof(*dgarm));
	((handle*)dgarm)->fd = fd;
	dgarm->recv_bufsize  = buffersize;
	dgarm->next_recv_buf = bytebuffer_new(buffersize);
	construct_datagram_socket(&dgarm->base);
	//save socket_ imp_engine_add,and replace with self
	if(!base_engine_add)
		base_engine_add = ((handle*)dgarm)->imp_engine_add; 
	((handle*)dgarm)->imp_engine_add = imp_engine_add;
	((socket_*)dgarm)->dctor = datagram_dctor;
	dgarm->decoder_ = d ? d:rawpacket_decoder_new();
	decoder_init(dgarm->decoder_,dgarm->next_recv_buf,0);
	return c;
}