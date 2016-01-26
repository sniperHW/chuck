#ifdef _CORE_

struct chk_acceptor {
    _chk_handle;
    void           *ud;      
    chk_acceptor_cb cb;
};

#endif