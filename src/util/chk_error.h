#ifndef _CHK_ERROR_H
#define _CHK_ERROR_H

#include <errno.h>

#ifndef EAGAIN 
#	define EAGAIN EWOULDBLOCK
#endif

//extend errno
enum{
    CHK_ESOCKCLOSE = 200,           /*socket close*/
    CHK_EPKTOOLARGE,                /*packet too large*/
    CHK_ERVTIMEOUT,                 /*recv timeout*/
    CHK_ESNTIMEOUT,                 /*send timeout*/
    CHK_ELOOPCLOSE,                 /*event_loop close*/
    CHK_ERDSPASERR,                 /*redis reply parse error*/
    CHK_EHTTP,                      /*http parse error*/
};

enum {
	chk_error_ok = 0,
	chk_error_socket_close,
	chk_error_timeout,
	chk_error_packet_too_large,
	chk_error_loop_close,
	chk_error_redis_reply_parse,
	chk_error_http_header_too_large,
	chk_error_http_content_too_large
};

#endif