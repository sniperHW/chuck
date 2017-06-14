#ifdef _CORE_

#include "../config.h"
#include "chk_ud.h"
#include "chk_send_cb.h"

struct chk_dgram_socket {
	_chk_handle;
	struct iovec         wsendbuf[MAX_WBAF];
    uint32_t             status;
    uint32_t             recvbuf_size;
    chk_ud               ud;        
    chk_list             send_list;             //待发送的包
    chk_list             send_cb_list;
    chk_list             finish_send_list; 
    /*
    *   发送定时器，用于检测发送阻塞（对于一个异步网络库，所有未能发送的数据都被放在发送队列中，
    *   如果对端不接收数据将导致发送方发送队列内存膨胀，为了避免这种情况，每当event_loop开始监听
    *   write事件，将启动一个定时器，如果定时器超时，向上层返回send_time_out事件，如果在定时器
    *   超时之前，有数据成功发送则清除定时器。
    */
    chk_timer           *send_timer;         
    chk_timer           *last_send_timer;       //用于最后的发送处理
    chk_dgram_socket_cb  cb;
    char                 recvbuf[0];
};

#endif