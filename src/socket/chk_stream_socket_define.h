#ifdef _CORE_

#include "../config.h"


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
    chk_list             urgent_list;           //紧急发送列表
    chk_timer           *timer;                 //用于最后的发送处理
    chk_stream_socket_cb cb;
    int8_t               create_by_new;
    int8_t               sending_urgent;        //标识当前是否正在发送urgent_list中的buffer
    //struct ssl_ctx       ssl;
};

#endif