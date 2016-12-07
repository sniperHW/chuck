local netcmd = {
	--client->server
	CMD_CS_LOGIN = 1,            --登录
	CMD_CS_ENTER_ROOM = 2,       --进房间请求
	CMD_CS_MOVE = 3,             --移动
	CMD_CS_KILL = 4,             --吞吃
	CMD_CS_RELIVE = 5,           --复活请求
	CMD_CS_LEAVE_ROOM = 6,       --离开房间请求
	CMD_CS_CLIENT_ENTER_ROOM = 7,--客户端进入房间

	--server->client
	CMD_SC_LOGIN = 100,
	CMD_SC_ENTER_ROOM = 101,   	
	CMD_SC_BEGIN_SEE = 102,      --看见一个
	CMD_SC_END_SEE = 103,        --消失一个
	CMD_SC_MOVE = 104,
	CMD_SC_LEAVE_ROOM = 105,

}

return netcmd