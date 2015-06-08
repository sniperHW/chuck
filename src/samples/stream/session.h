struct session;
typedef struct {
	iorequest base;
	struct session *s;
}my_ioreq;


struct session{
	my_ioreq send_overlap;
	my_ioreq recv_overlap;
	struct iovec wbuf;
	char         buf[1024];
	stream_socket_   *s;
};

int32_t session_recv(struct session *s,int flag)
{
	s->wbuf.iov_base = s->buf;
	s->wbuf.iov_len = 1024;
	s->recv_overlap.base.iovec_count = 1;
	s->recv_overlap.base.iovec = &s->wbuf;
	return stream_socket_recv(s->s,(iorequest*)&s->recv_overlap,flag);
}

int32_t session_send(struct session *s,int32_t size,int flag)
{
	s->wbuf.iov_base = s->buf;
   	s->wbuf.iov_len = size;
	s->send_overlap.base.iovec_count = 1;
	s->send_overlap.base.iovec = &s->wbuf;
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
        close_socket((socket_*)h);
        free(s);
        --client_count;           
        return;
    }

    if(req == &s->send_overlap)
		session_recv(s,IO_POST);
    if(req == &s->recv_overlap){
		session_send(s,bytestransfer,IO_POST);
		totalbytes += bytestransfer;
		packet_recv++;
	}
}

struct session *session_new(stream_socket_ *h){
	struct session *s = calloc(1,sizeof(*s));
	s->s = h;
	s->send_overlap.s = s;
	s->recv_overlap.s = s;
	return s;
}
