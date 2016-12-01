#ifndef _CHK_ERROR_H
#define _CHK_ERROR_H

#include <errno.h>

#ifndef EAGAIN 
#	define EAGAIN EWOULDBLOCK
#endif

enum {
	chk_error_ok = 0,                                     /*无错误*/
	chk_error_common,                                     /*通用错误*/
	chk_error_no_memory,                                  /*内存不足*/
	chk_error_invaild_argument,                           /*非法参数*/
	chk_error_create_notify_channel,                      /*创建通告管道出错*/
	chk_error_invaild_iterator,                           /*非法迭代器*/
	chk_error_iterate_end,                                /*迭代到达末尾*/
	chk_error_invaild_size,                               /*size参数大小非法*/
	chk_error_eof,                                        /*读到文件结尾*/
	chk_error_add_timer,
	/*buffef相关错误码*/
	chk_error_buffer_read_only,                           /*只读buffer*/
	chk_error_invaild_buffer,                             /*非法buffer*/
	/*chk_signal相关错误码*/
	chk_error_create_signal_handler,                      /*创建信号处理器失败*/
	chk_error_duplicate_signal_handler,                   /*重复的信号处理器*/
	chk_error_forbidden_signal,                           /*禁止watch的信号*/
	/*event_loop相关错误码*/
	chk_error_duplicate_add_handle,                       /*重复添加handle*/
	chk_error_epoll_add,                                  /*调用epoll_add失败*/
	chk_error_no_event_loop,                              /*没有关联event_loop*/
	chk_error_epoll_del,                                  /*调用epoll_del失败*/
	chk_error_epoll_mod,                                  /*调用epoll_mod失败*/ 
	chk_error_create_epoll,                               /*创建epollfd失败*/
	chk_error_kevent_add,
	chk_error_kevent_enable,
	chk_error_kevent_disable,
	chk_error_create_kqueue,
	chk_error_loop_run,
	chk_error_loop_close,                                 /*event loop关闭*/ 
	/*redis相关错误码*/
	chk_error_redis_request,                              /*redis请求执行失败*/ 
	chk_error_redis_parse,                                /*redis响应解析失败*/
	
	/*socket相关错误码*/
	chk_error_bind,
	chk_error_listen,
	chk_error_accept,
	chk_error_connect,
	chk_error_invaild_addr_type,
	chk_error_setsockopt,
	chk_error_fcntl,
	chk_error_invaild_sockaddr,
	chk_error_invaild_hostname,
	chk_error_connect_timeout,                            /*connect超时*/
	chk_error_socket_close,                               /*socket已经关闭*/                                                     
	chk_error_stream_write,
	chk_error_stream_read,
	chk_error_stream_peer_close,
	/*packet相关错误码*/
	chk_error_packet_too_large,                           /*数据包太大*/
	chk_error_invaild_packet_size,                        /*数据包长度非法*/
	chk_error_http_packet,                                /*http解包错误*/
	chk_error_unpack,                                     /*解包错误*/
};

#endif
