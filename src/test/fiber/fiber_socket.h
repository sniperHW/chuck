#include "chuck.h"

typedef chk_refhandle fiber_socket_t;

int                fiber_socket_init(chk_event_loop*);
chk_acceptor*      fiber_socket_listen(const char *ip,int16_t port);
fiber_socket_t 	   fiber_socket_accept(chk_acceptor);
fiber_socket_t 	   fiber_socket_connect(chk_sockaddr *remote);
int                fiber_socket_recv(fiber_socket_t,chk_bytebuffer*,int *err);
int                fiber_socket_send(fiber_socket_t,chk_bytebuffer);
int                fiber_socket_close(fiber_socket_t);
void               fiber_socket_socket_run();
