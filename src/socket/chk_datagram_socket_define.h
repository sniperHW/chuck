#ifdef _CORE_

#include "../config.h"
#include "chk_ud.h"

struct chk_datagram_socket {
	_chk_handle;
	struct iovec           wsendbuf[MAX_WBAF];
    char                   recvbuff[65535];
    chk_ud                 ud;        
    chk_datagram_socket_cb cb;
    int8_t                 closed;
    int8_t                 inloop;
    int8_t                 boradcast;
    int                    addr_type;    
};

#endif