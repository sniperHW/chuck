#ifdef _CORE_

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "chk_ud.h"

struct chk_acceptor {
    _chk_handle;
    chk_ud          ud; 
    chk_acceptor_cb cb;
    SSL_CTX        *ctx;
};

#endif