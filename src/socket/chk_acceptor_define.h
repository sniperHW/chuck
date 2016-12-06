#ifdef _CORE_

#include <openssl/ssl.h>
#include <openssl/err.h>

struct chk_acceptor {
    _chk_handle;
    void           *ud; 
    union{      
    	chk_acceptor_cb      cb;
    	chk_ssl_acceptor_cb  ssl_cb;
    };
    SSL_CTX        *ctx;
};

#endif