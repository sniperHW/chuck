#ifdef _CORE_

#include <openssl/ssl.h>
#include <openssl/err.h>

struct chk_acceptor {
    _chk_handle;
    void           *ud; 
    chk_acceptor_cb cb;
    SSL_CTX        *ctx;
};

#endif