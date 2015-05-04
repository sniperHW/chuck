#include "socket/wrap/datagram.h"
#include "engine/engine.h"
#include "packet/rawpacket.h"

enum{
	RECVING   = 1 << 5,
};

static int32_t (*base_engine_add)(engine*,struct handle*,generic_callback) = NULL;

int32_t datagram_send(datagram *d,packet *p,sockaddr_ *addr)
{
	iorequest   o;
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
		ret = datagram_socket_send((handle*)d,&o);
	}while(0);
	packet_del(p);
	return ret;
}

static inline void prepare_recv(datagram *d){
	bytebuffer *buf;
	int32_t     i = 0;
	uint32_t    free_buffer_size,recv_size,pos;
	if(!d->next_recv_buf){
		d->next_recv_buf = bytebuffer_new(d->recv_bufsize);
		d->next_recv_pos = 0;
	}
	buf = d->next_recv_buf;
	pos = d->next_recv_pos;
	recv_size = d->recv_bufsize;
	do
	{
		free_buffer_size = buf->cap - pos;
		free_buffer_size = recv_size > free_buffer_size ? free_buffer_size:recv_size;
		d->wrecvbuf[i].iov_len = free_buffer_size;
		d->wrecvbuf[i].iov_base = buf->data + pos;
		recv_size -= free_buffer_size;
		pos += free_buffer_size;
		if(recv_size && pos >= buf->cap)
		{
			pos = 0;
			if(!buf->next)
				buf->next = bytebuffer_new(d->recv_bufsize);
			buf = buf->next;
		}
		++i;
	}while(recv_size);
	d->recv_overlap.iovec_count = i;
	d->recv_overlap.iovec = d->wrecvbuf;
}

static inline void PostRecv(datagram *d){
	((socket_*)d)->status |= RECVING;
	prepare_recv(d);
	datagram_socket_recv((handle*)d,&d->recv_overlap,IO_POST,NULL);		
}

static inline void update_next_recv_pos(datagram *d,int32_t _bytestransfer)
{
	assert(_bytestransfer >= 0);
	uint32_t bytestransfer = (uint32_t)_bytestransfer;
	uint32_t size;
	decoder *decoder_ = d->decoder_;
	if(!decoder_->buff)
		decoder_init(decoder_,d->next_recv_buf,d->next_recv_pos);
	decoder_->size += bytestransfer;
	do{
		size = d->next_recv_buf->cap - d->next_recv_pos;
		size = size > bytestransfer ? bytestransfer:size;
		d->next_recv_buf->size += size;
		d->next_recv_pos += size;
		bytestransfer -= size;
		if(d->next_recv_pos >= d->next_recv_buf->cap)
		{
			bytebuffer_set(&d->next_recv_buf,d->next_recv_buf->next);
			d->next_recv_pos = 0;
		}
	}while(bytestransfer);
}

static void IoFinish(handle *sock,void *_,int32_t bytestransfer,int32_t err_code,int32_t recvflags)
{
	int32_t unpack_err;
	datagram *d = (datagram*)sock;	
	if(bytestransfer > 0 && recvflags != MSG_TRUNC){
		update_next_recv_pos(d,bytestransfer);
		packet *pk = d->decoder_->unpack(d->decoder_,&unpack_err);
		if(pk){
			d->on_packet(d,pk,&d->recv_overlap.addr);
			packet_del(pk);
			if(((socket_*)d)->status & SOCKET_CLOSE)
				return;
		}
	}
	PostRecv(d);
}


static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback){
	int32_t ret;
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	//call the base_engine_add first
	ret = base_engine_add(e,h,(generic_callback)IoFinish);
	if(ret == 0){
		((datagram*)h)->on_packet = (void(*)(datagram*,packet*,sockaddr_*))callback;
		//post the first recv request
		if(!(((socket_*)h)->status & RECVING))
			PostRecv((datagram*)h);
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
	dgarm->decoder_ = d ? d:dgram_raw_decoder_new();
	decoder_init(dgarm->decoder_,dgarm->next_recv_buf,0);
	return dgarm;
}

static packet *rawpk_unpack(decoder *d,int32_t *err){
	rawpacket    *raw;
	uint32_t      size;
	buffer_writer writer;
	bytebuffer   *buff;
	if(err) *err = 0;
	if(!d->size) return NULL;
	raw = rawpacket_new(d->size);
	buffer_writer_init(&writer,((packet*)raw)->head,0);
	((packet*)raw)->len_packet = d->size;
	while(d->size){
		buff = d->buff;
		size = buff->size - d->pos;
		buffer_write(&writer,buff->data + d->pos,size);
		d->size -= size;
		d->pos += size;
		if(d->pos >= buff->cap){
			d->pos = 0;
			bytebuffer_set(&d->buff,buff->next);
		}
	}
	return (packet*)raw;
}

decoder *dgram_raw_decoder_new(){
	decoder *d = calloc(1,sizeof(*d));
	d->unpack = rawpk_unpack;
	return d;	
}