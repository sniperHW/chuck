#ifndef _CHK_ERROR_H
#define _CHK_ERROR_H

#include <errno.h>

#ifndef EAGAIN 
#	define EAGAIN EWOULDBLOCK
#endif


#define CHUCK_ERRNO_MAP(XX)													\
																			\
	XX(0,chk_error_ok)														\
	XX(1,chk_error_common)                                     				\
	XX(2,chk_error_no_memory)												\
	XX(3,chk_error_invaild_argument)                           				\
	XX(4,chk_error_create_notify_channel)                      				\
	XX(5,chk_error_invaild_iterator)                           				\
	XX(6,chk_error_iterate_end)                                				\
	XX(7,chk_error_invaild_size)                               				\
	XX(8,chk_error_eof)                                       				\
	XX(9,chk_error_add_timer)												\
	XX(10,chk_error_buffer_read_only)                           			\
	XX(11,chk_error_invaild_buffer)                             			\
	XX(12,chk_error_invaild_pos)											\
	XX(13,chk_error_create_signal_handler)                      			\
	XX(14,chk_error_duplicate_signal_handler)                  				\
	XX(15,chk_error_forbidden_signal)                           			\
	XX(16,chk_error_duplicate_add_handle)                       			\
	XX(17,chk_error_epoll_add)                                				\
	XX(18,chk_error_no_event_loop)                              			\
	XX(19,chk_error_epoll_del)                                 				\
	XX(20,chk_error_epoll_mod)                                   			\
	XX(21,chk_error_create_epoll)                               			\
	XX(22,chk_error_kevent_add)												\
	XX(23,chk_error_kevent_enable)											\
	XX(24,chk_error_kevent_disable)											\
	XX(25,chk_error_create_kqueue)											\
	XX(26,chk_error_loop_run)												\
	XX(27,chk_error_loop_close)												\
	XX(28,chk_error_redis_request)                              			\
	XX(29,chk_error_redis_parse)                                			\
	XX(30,chk_error_redis_timeout)											\
	XX(31,chk_error_bind)													\
	XX(32,chk_error_listen)													\
	XX(33,chk_error_accept)													\
	XX(34,chk_error_connect)												\
	XX(35,chk_error_invaild_addr_type)										\
	XX(36,chk_error_setsockopt)												\
	XX(37,chk_error_fcntl)													\
	XX(38,chk_error_invaild_sockaddr)										\
	XX(39,chk_error_invaild_hostname)										\
	XX(40,chk_error_connect_timeout)                            			\
	XX(41,chk_error_socket_close)                                          	\
	XX(42,chk_error_stream_write)											\
	XX(43,chk_error_stream_read)											\
	XX(44,chk_error_stream_peer_close)										\
	XX(45,chk_error_send_timeout)											\
	XX(46,chk_error_send_failed)											\
	XX(47,chk_error_ssl_error)												\
	XX(48,chk_error_highwater_mark)											\
	XX(49,chk_error_msg_trunc)												\
	XX(50,chk_error_packet_too_large)                           			\
	XX(51,chk_error_invaild_packet_size)                        			\
	XX(52,chk_error_http_packet)                                			\
	XX(53,chk_error_unpack)    												\
	XX(54,chk_error_dgram_read)												\
	XX(55,chk_error_dgram_set_boradcast)                                    \
	XX(56,chk_error_dgram_boradcast_flag)

enum 
  {
#define XX(num,name) name = num,
  CHUCK_ERRNO_MAP(XX)
#undef XX
  };

const char *chk_get_errno_str(int);


#endif
