#ifdef _CORE_

#define MAX_WBAF          1024

#define MAX_SEND_SIZE     1024*64

struct chk_stream_socket {
	_chk_handle;
	chk_stream_socket_option option;
	struct iovec         wsendbuf[MAX_WBAF];
    struct iovec         wrecvbuf[2];
    uint32_t             status;
    uint32_t             next_recv_pos;
    chk_bytechunk       *next_recv_buf;
    void                *ud;        
    chk_list             send_list;             //待发送的包
    chk_timer           *timer;                 //用于最后的发送处理
    chk_stream_socket_cb cb;
    uint8_t              create_by_new;
};

#endif