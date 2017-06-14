#include <stdio.h>
#include "kcp/ikcp.h"
#include "util/chk_list.h"
#include "util/chk_bytechunk.h"
#include "util/chk_time.h"

chk_list client_to_server;
chk_list server_to_client;

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	chk_list *channel = (chk_list*)user;
	chk_bytebuffer *buff = chk_bytebuffer_new(len);
	chk_bytebuffer_append(buff,(uint8_t*)buf,len);
	chk_list_pushback(channel,(chk_list_entry*)buff);
	return 0;
}


void process_kcp(ikcpcb *kcp,chk_list *recv_channel) {
	ikcp_update(kcp, chk_systick32());

	char buff[100];

	while (!chk_list_empty(recv_channel)) {
		chk_bytebuffer *msg = (chk_bytebuffer*)chk_list_pop(recv_channel);
		uint32_t len = chk_bytebuffer_read(msg,buff,sizeof(buff));
		chk_bytebuffer_del(msg);
		ikcp_input(kcp, buff, len);
	}

	while (1) {
		int len = ikcp_recv(kcp, buff, sizeof(buff));
		// 没有收到包就退出
		if (len < 0) break;

		if(recv_channel == &client_to_server) {
			printf("%s\n",buff);
		}

		// 如果收到包就回射
		ikcp_send(kcp, buff, len);
	}
}


int main() {

	chk_list_init(&client_to_server);
	chk_list_init(&server_to_client);

	ikcpcb *kcp_server = ikcp_create(0x11223344, (void*)&server_to_client);
	ikcpcb *kcp_client = ikcp_create(0x11223344, (void*)&client_to_server);

	// 设置kcp的下层输出，这里为 udp_output，模拟udp网络输出函数
	kcp_server->output = udp_output;
	kcp_client->output = udp_output;

	ikcp_wndsize(kcp_server, 128, 128);
	ikcp_wndsize(kcp_client, 128, 128);

	const char hello[] = "hello";
	ikcp_send(kcp_client,hello,sizeof(hello));

	while(1) {
		chk_sleepms(100);
		process_kcp(kcp_server,&client_to_server);
		process_kcp(kcp_client,&server_to_client);
	}

	return 0;
}