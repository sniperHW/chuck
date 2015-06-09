struct session;
typedef struct {
	iorequest base;
	struct session *s;
}my_ioreq;


struct session{
	my_ioreq send_overlap;
	my_ioreq recv_overlap;
	struct iovec wrbuf;
	struct iovec wsbuf;
	char         rbuf[1024];
	char         sbuf[1024];
	stream_socket_   *s;
};

int32_t session_recv(struct session *s,int flag)
{
	s->wrbuf.iov_base = s->rbuf;
	s->wrbuf.iov_len = 1024;
	s->recv_overlap.base.iovec_count = 1;
	s->recv_overlap.base.iovec = &s->wrbuf;
	return stream_socket_recv(s->s,(iorequest*)&s->recv_overlap,flag);
}

int32_t session_send(struct session *s,int32_t size,int flag)
{
	s->wsbuf.iov_base = s->sbuf;
   	s->wsbuf.iov_len = size;
	s->send_overlap.base.iovec_count = 1;
	s->send_overlap.base.iovec = &s->wsbuf;
	return stream_socket_send(s->s,(iorequest*)&s->send_overlap,flag);  	 
}

int      client_count = 0;
double   totalbytes   = 0;
int      packet_recv  = 0;


void transfer_finish(handle *h,void *_,int32_t bytestransfer,int32_t err){
    my_ioreq *req = ((my_ioreq*)_);
    struct session *s = req->s; 
    if(!req || bytestransfer <= 0)
    {
socket_err:      	
        close_socket((socket_*)h);
        free(s);
        --client_count;           
        return;
    }

    if(req == &s->send_overlap)
		session_recv(s,IO_POST);
    else{
		totalbytes += bytestransfer;
		packet_recv++;
    	memcpy(s->sbuf,s->rbuf,bytestransfer);
		bytestransfer = session_send(s,bytestransfer,IO_NOW);				
		if(bytestransfer >= 0)
			session_recv(s,IO_POST);
		else if(bytestransfer < 0 && bytestransfer != -EAGAIN)
			goto socket_err;
	}
}

struct session *session_new(stream_socket_ *h){
	struct session *s = calloc(1,sizeof(*s));
	s->s = h;
	s->send_overlap.s = s;
	s->recv_overlap.s = s;
	return s;
}
