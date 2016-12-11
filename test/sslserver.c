#include <stdio.h>
#include "chuck.h"

chk_event_loop *loop;

chk_stream_socket_option option = {
	.recv_buffer_size = 4096,
	.decoder = NULL,
};

void data_event_cb(chk_stream_socket *s,chk_bytebuffer *data,int32_t error) {
	if(data){
		printf("recv request:%s\n",data->head->data);
		chk_stream_socket_send(s,chk_bytebuffer_clone(data),NULL,NULL);
	}
	else{
		printf("socket close:%d\n",error);
	}
}

void accept_cb_ssl(chk_acceptor *a,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err) {
	
	if(fd >= 0) {

		chk_stream_socket *s = chk_stream_socket_new(fd,&option);

		SSL_CTX *ssl_ctx = chk_acceptor_get_ssl_ctx(a);

		if(0 == chk_ssl_accept(s,ssl_ctx)){
			chk_loop_add_handle(loop,(chk_handle*)s,data_event_cb);
		}
		else {
			chk_stream_socket_close(s,0);
			printf("chk_ssl_accept error\n");
		}
	}
	else {
		printf("accept error:%d\n",err);
	}
}

static void signal_int(void *ud) {
	printf("signal_int\n");
	chk_loop_end(loop);
}

int main(int argc,char **argv) {

	const char *certificate = "./test/cacert.pem";
	const char *privatekey = "./test/privkey.pem";

	signal(SIGPIPE,SIG_IGN);
	loop = chk_loop_new();

	if(0 != chk_watch_signal(loop,SIGINT,signal_int,NULL,NULL)) {
		printf("watch signal failed\n");
		return 0;
	}

    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    /* 也可以用 SSLv2_server_method() 或 SSLv3_server_method() 单独表示 V2 或 V3标准 */
    if (ctx == NULL) {
        ERR_print_errors_fp(stdout);
        return 0;
    }
    /* 载入用户的数字证书， 此证书用来发送给客户端。 证书里包含有公钥 */
    if (SSL_CTX_use_certificate_file(ctx,certificate, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stdout);
        return 0;
    }
    /* 载入用户私钥 */
    if (SSL_CTX_use_PrivateKey_file(ctx, privatekey, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stdout);
        return 0;
    }
    /* 检查用户私钥是否正确 */
    if (!SSL_CTX_check_private_key(ctx)) {
        ERR_print_errors_fp(stdout);
        return 0;
    }	

	if(!chk_ssl_listen_tcp_ip4(loop,argv[1],atoi(argv[2]),ctx,accept_cb_ssl,NULL)){
		printf("server start error\n");
	}
	else{
		CHK_SYSLOG(LOG_INFO,"server start");
		chk_loop_run(loop);
	}

	chk_loop_del(loop);
	return 0;
}