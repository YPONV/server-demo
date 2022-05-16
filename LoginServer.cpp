#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string.h>
#include <queue>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "project.pb.h"
#include <sys/ipc.h>
#include <pthread.h>
#include <sys/shm.h>
#include "MsgQueue.h"
#include "CEpoll.h"
#include "SharedMemory.h"
using namespace std;
MsgQueue* MSG;
typedef struct _data_buf
{
	int fd;
	char buf[_BUF_SIZE_];
}data_buf_t,*data_buf_p;
int sockfd[5];
string IntToString(int num)
{
	string str = "";
	if(num == 0)
	{
		str += "0";
		return str;
	}
	while(num)
	{
		str += num%10+'0';
		num/=10;
	}
	reverse(str.begin(),str.end());
	return str;
}
int StringToInt(string str)
{
	int fd = 0;
	for(int i = 0; i < str.size(); i ++ )
	{
		fd = fd * 10 + str[i] - '0';
	}
	return fd;
}
void StringToProtobuf(Account& nAccount, string& Message)
{
	char p[1024];
	memset(p, '\0', sizeof p);
	for(int j = 0; j < Message.size(); j ++){
		p[j] = Message[j];
	}
	nAccount.ParseFromArray(p, Message.size());
}
long long Gettime()
{
    struct timeval tv;
	gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
void AddPack(char *Newdata,char *data, int len)//给消息包加头
{
	int len1=len;
	for(int i = 3; i >= 0; i --)
	{
		if(len1 > 0)
		{
			Newdata[i] = len1 % 10 + '0';
			len1 /= 10;
		}
		else Newdata[i] = '0';
	}
	for(int i = 0; i < len; i ++)
	{
		Newdata[i + 4] = data[i];
	}
}
int SendMessage(Account& nAccount,int fd)//flag记录是什么事件
{
	char p[1024],pp[1024];
	memset(p,'\0',sizeof p);
	memset(pp,'\0',sizeof pp);
	int sz = nAccount.ByteSize();
	nAccount.SerializeToArray(p, sz);
	AddPack(pp, p, sz);
	sz += 4;
	char* ptr = pp;
	long long T = Gettime();
	while(sz > 0)
	{
		long long now = Gettime();
		if(now - T > 2)
		{
			return -1;
		}
		int written_bytes = send(fd, ptr, sz,0);
		if(written_bytes < 0)
        {       
            printf("SendMessage error!\n");
        }
        sz -= written_bytes;
        ptr += written_bytes;     
	}
}
static void * Rpthread(void *arg)//接收来自GateServce的消息
{
    string Message = "";
	int flag = 0, number = 0, id = 0;
	int sockfd;
	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
	}

	struct hostent* h;
	if( (h = gethostbyname("10.0.128.212")) == 0)
	{
		printf("gethostbyname failed,\n");
		close(sockfd);
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi("1111"));
	memcpy(&servaddr.sin_addr, h->h_addr, h->h_length);
	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
	{
		perror("connect");
		close(sockfd);
	}
	char buffer[1024];
	while(1)
	{
		int iret, iret1;
		memset(buffer, 0, sizeof(buffer));
		if( (iret = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0)
		{
			printf("iret=%d\n",iret);
			break;
		}
		for(int i = 0; i < iret; i ++ )
		{
			if(number > 0 && id == 4)
			{
			    Message += buffer[i];
			    number --;
			}
			if(number == 0 && id == 4)
			{
				Account nAccount;
				StringToProtobuf(nAccount, Message);//将string反序列化
				if(MSG->MsgQueue_Wait())
        		{
					MSG->que.push(Message);
            		MSG->MsgQueue_Close();
        		}
			    Message = "";
			    id = 0;
			    continue;
 			}
            if(id < 4)
			{
		        number = number*10+buffer[i]-'0';
			    id ++;
			}
		}
		sleep(0.03);	
	}
}
static void * Spthread(void *arg)//给GateServce发送消息
{
	int listenfd;
	if( (listenfd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		perror("socket");
	}
	struct sockaddr_in servaddr;
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;//协议族，在socket编程中只能是AF_INET。
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//任意ip地址
	//servaddr.sin_addr.s_addr = inet_addr("192.168.190.134");//指定ip地址
	servaddr.sin_port = htons(atoi("123456"));//指定通信端口
	//把socket设置为监听模式
	if (bind(listenfd,(struct sockaddr *)& servaddr,sizeof(servaddr)) !=0)
	{
		perror("bind");
		close(listenfd);
	}
	if(listen(listenfd,1)!=0)
	{
		perror("listen");
		close(listenfd);
	}
	//接受客户端的连接
	int clientfd;
	int socklen=sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	clientfd = accept(listenfd,(struct sockaddr *)&clientaddr,(socklen_t*)&socklen);
    while(1)
    {
        if(MSG->MsgQueue_Wait())
        {

            MSG->MsgQueue_Close();
        }
        sleep(0.03);
    }
}
int main()
{
    string Message[2];
	Message[0] = Message[1] = "";
	int number[2] = {0}, id[2] = {0};
	if( (sockfd[0] = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
	}
	struct hostent* h;
	if( (h = gethostbyname("10.0.128.212")) == 0)
	{
		printf("gethostbyname failed,\n");
		close(sockfd[0]);
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi("1111"));
	memcpy(&servaddr.sin_addr, h->h_addr, h->h_length);
	if(connect(sockfd[0], (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
	{
		perror("connect");
		close(sockfd[0]);
	}
	
	if( (sockfd[1] = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
	}
	if( (h = gethostbyname("10.0.128.212")) == 0)
	{
		printf("gethostbyname failed,\n");
		close(sockfd[1]);
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi("2222"));
	memcpy(&servaddr.sin_addr, h->h_addr, h->h_length);
	if(connect(sockfd[1], (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
	{
		perror("connect");
		close(sockfd[1]);
	}
	int a[1] ={1};
	char buffer[1024];
	while(1)
	{
		int iret;
		memset(buffer, 0, sizeof(buffer));
		if( (iret = recv(sockfd[0], buffer, sizeof(buffer), MSG_DONTWAIT) ) == 0)
		{
			printf("iret=%d\n",iret);
			break;
		}
		setsockopt(sockfd[0], IPPROTO_TCP, TCP_QUICKACK, a, sizeof(int));
		for(int i = 0; i < iret; i ++ )
		{
			if(number[0] > 0 && id[0] == 4)
			{
			    Message[0] += buffer[i];
			    number[0] --;
			}
			if(number[0] == 0 && id[0] == 4)
			{
				Account nAccount;
                StringToProtobuf(nAccount, Message[0]);
				SendMessage(nAccount, sockfd[1]);
				Message[0] = "";
				id[0] = 0;
				continue;
			}
			if(id[0] < 4)
			{
				number[0] = number[0] * 10 + buffer[i] - '0';
				id[0] ++;
			}
		}

		memset(buffer, 0, sizeof(buffer));
		if( (iret = recv(sockfd[1], buffer, sizeof(buffer), MSG_DONTWAIT) ) == 0)
		{
			printf("iret=%d\n",iret);
			break;
		}
		setsockopt(sockfd[1], IPPROTO_TCP, TCP_QUICKACK, a, sizeof(int));
		for(int i = 0; i < iret; i ++ )
		{
			if(number[1] > 0 && id[1] == 4)
			{
			    Message[1] += buffer[i];
			    number[1] --;
			}
			if(number[1] == 0 && id[1] == 4)
			{
				Account nAccount;
                StringToProtobuf(nAccount, Message[1]);
				SendMessage(nAccount, sockfd[0]);
				Message[1] = "";
				id[1] = 0;
				continue;
			}
			if(id[1] < 4)
			{
				number[1] = number[1] * 10 + buffer[i] - '0';
				id[1] ++;
			}
		}
	}
}
