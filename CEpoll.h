#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define _BACKLOG_ 5
#define _BUF_SIZE_ 1024
#define _MAX_ 64

class CEpoll
{
public:
  int m_listenfd;   // 服务端用于监听的socket
 
  CEpoll();
  ~CEpoll();

  int listen_sock;
  //epoll连接
  int start(int port, char *ip);
  
};