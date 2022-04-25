#include "CEpoll.h"

CEpoll::CEpoll()
{
	
}
CEpoll::~CEpoll()
{
	close(m_listenfd);
}
int CEpoll::start(int port, char *ip)
{
    assert(ip);
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("socket");
		exit(1);
	}
	//地址
	struct sockaddr_in local;
	//本地到网络流的转换
	local.sin_port = htons(port);
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = inet_addr(ip);

	int opt=1;//设置为接口复用
	//打开sock的地址复用功能
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0)
	{
		perror("bind");
		exit(2);
	}
	if(listen(sock,_BACKLOG_) < 0)
	{
		perror("listen");
		exit(3);
	}
	return sock;
}
