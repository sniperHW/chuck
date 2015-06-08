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

int32_t session_recv(struct session *s,int flag)
{
	s->wbuf[0].iov_base = s->buf;
	s->wbuf[0].iov_len = 65535;
	s->recv_overlap.base.iovec_count = 1;
	s->recv_overlap.base.iovec = s->wbuf;
	return stream_socket_recv(s->s,(iorequest*)&s->recv_overlap,flag);
}

int32_t session_send(struct session *s,int32_t size,int flag)
{
	s->wbuf[0].iov_base = s->buf;
   	s->wbuf[0].iov_len = size;
	s->send_overlap.base.iovec_count = 1;
	s->send_overlap.base.iovec = s->wbuf;
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
//socket_err:    	
        close_socket((socket_*)h);
        free(s);
        --client_count;           
        return;
    }

/*    do{
    	if(req == &s->send_overlap){
    		bytestransfer = session_recv(s,IO_NOW);
    		if(bytestransfer == 0 ||
    		  (bytestransfer < 0 && bytestransfer != -EAGAIN))	
    			goto socket_err;
    		req = &s->recv_overlap;
    	}else{
			totalbytes += bytestransfer;
			packet_recv++;    		
    		bytestransfer = session_send(s,bytestransfer,IO_NOW);
    		if(bytestransfer == 0 ||
    		  (bytestransfer < 0 && bytestransfer != -EAGAIN))	
    			goto socket_err;
    		req = &s->send_overlap;
    	}
    }while(bytestransfer > 0);
*/
    //if(req == &s->send_overlap)
		
    if(req == &s->recv_overlap){
    	session_recv(s,IO_POST);
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
