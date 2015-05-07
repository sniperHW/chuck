struct session;
typedef struct {
	iorequest base;
	struct session *s;
}my_ioreq;


struct session{
	my_ioreq send_overlap;
	my_ioreq recv_overlap;
	struct iovec wbuf[1];
	char   	  buf[65535];
	stream_socket_   *s;
};

void session_recv(struct session *s)
{
	s->wbuf[0].iov_base = s->buf;
	s->wbuf[0].iov_len = 65535;
	s->recv_overlap.base.iovec_count = 1;
	s->recv_overlap.base.iovec = s->wbuf;
	stream_socket_recv(s->s,(iorequest*)&s->recv_overlap,IO_POST);
}

void session_send(struct session *s,int32_t size)
{
	s->wbuf[0].iov_base = s->buf;
   	s->wbuf[0].iov_len = size;
	s->send_overlap.base.iovec_count = 1;
	s->send_overlap.base.iovec = s->wbuf;
	stream_socket_send(s->s,(iorequest*)&s->send_overlap,IO_POST);  	 
}

int      client_count = 0;
double   totalbytes   = 0;


void transfer_finish(handle *h,void *_,int32_t bytestransfer,int32_t err){
    my_ioreq *req = ((my_ioreq*)_);
    struct session *s = req->s; 
    if(!req || bytestransfer <= 0)
    {
        close_socket((socket_*)h);
        free(s);
        --client_count;           
        return;
    }	
    if(req == &s->send_overlap)
		session_recv(s);
    else if(req == &s->recv_overlap){
		session_send(s,bytestransfer);
		totalbytes += bytestransfer;
	}
}

struct session *session_new(stream_socket_ *h){
	struct session *s = calloc(1,sizeof(*s));
	s->s = h;
	s->send_overlap.s = s;
	s->recv_overlap.s = s;
	return s;
}
