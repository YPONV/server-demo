#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string.h>
#include <queue>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
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
MsgQueue* RMSG;
MsgQueue* SMSG;
typedef struct _data_buf
{
	int fd;
	char buf[_BUF_SIZE_];
}data_buf_t,*data_buf_p;
map<int, int> LengthMap;//通过fd获取剩余需要读取的消息长度
map<int, string> MessageMap;//通过fd得到缓存的消息
map<int, int> indexMap;//通过fd得知目前消息头读到了第几位
int FD[3];//FD[1]表示LoginServer进程的fd，FD[2]表示GameServer进程的fd
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
void SendMessage(Account& nAccount,int fd)//单独发送消息
{
	char p[1024],pp[1024];
	memset(p,'\0',sizeof p);
	memset(pp,'\0',sizeof pp);
	int sz = nAccount.ByteSize();
	nAccount.SerializeToArray(p, sz);
	AddPack(pp, p, sz);
	char* ptr = pp;
	while(sz > 0)
	{
		int written_bytes = write(fd, ptr, sz + 4);
		if(written_bytes < 0)
        {       
            printf("SendMessage error!\n");
        }
        sz -= written_bytes;
        ptr += written_bytes;     
	}
}
void epoll_servce(int listen_sock)
{
	int epoll_fd = epoll_create(256);
	if(epoll_fd < 0)	
	{
		perror("perror_create");
		exit(1);
	}
	//用于注册事件
	struct epoll_event ev;
	//数组用于回传要处理的事件
	struct epoll_event ret_ev[_MAX_];
	int ret_num = _MAX_;
	int read_num = -1;
	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &ev) < 0)
	{
		perror("epoll_ctl");
		exit(1);
	}
	int timeout = 1;
    int done = 0;
	int now = 0;//目前是谁登录
	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	struct timeval tv;
	//printf("before=%ld\n",tv.tv_sec);
	while(!done)
	{
		switch(read_num = epoll_wait(epoll_fd, ret_ev, ret_num, timeout))
		{
			case 0:
				break;
			case -1:
				printf("epoll\n");
				exit(2);
			default:
			{
				for(int i = 0; i < read_num; i++)
				{
					if(ret_ev[i].data.fd == listen_sock && (ret_ev[i].events & EPOLLIN))
					{
						int fd = ret_ev[i].data.fd;
					    int new_sock = accept(fd, (struct sockaddr *)&client, &len);
						if (new_sock < 0)
						{
							perror("accept");
							continue;					
						}
						if(now<2)
						{
							now ++;
							FD[now] = new_sock; 
						}
						ev.events = EPOLLIN;
						ev.data.fd = new_sock;
                        LengthMap[new_sock] = 0;
                        MessageMap[new_sock] = "";
                        indexMap[new_sock] = 0;
						epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_sock, &ev);
						//printf("get a new client\n");
					}
					else{
						if(ret_ev[i].events & EPOLLIN)
						{
							int fd = ret_ev[i].data.fd;
							data_buf_p mem = (data_buf_p)malloc(sizeof(data_buf_t));
							if(!mem)	
							{
								printf("malloc");
								continue;
							}
							mem->fd = fd;
							memset(mem->buf, '\0', sizeof(mem->buf));
							ssize_t _s = read(mem->fd, mem->buf, sizeof(mem->buf) - 1);
							if(_s > 0)
							{
                                char p[1024];
								memset(p, '\0' ,sizeof p);
								for(int i = 0; i < _s; i ++)
								{
									if(LengthMap[fd] > 0 && indexMap[fd] == 4)
									{
										MessageMap[fd] += mem->buf[i];
										LengthMap[fd] --;
									}
									//一条数据接收完毕
									if(LengthMap[fd] == 0 && indexMap[fd] == 4)
									{
										for(int j = 0; j < MessageMap[fd].size(); j ++){
											p[j] = MessageMap[fd][j];
										}
										Account nAccount;
										nAccount.ParseFromArray(p, MessageMap[fd].size());
										if(nAccount.move() == 1 || nAccount.move() == 2)//登录或注册
										{
											if(fd == FD[1])//从DBServer发回来的
											{
												SendMessage(nAccount, nAccount.fd());//发回客户端
												if(nAccount.move() == 1 && nAccount.flag() == true)//登录成功则发送发到GameServer
												{
													SendMessage(nAccount, FD[2]);//发送到GameServer
												}
											}
											else//从客户端发来的
											{	
												nAccount.set_fd(fd);			
												SendMessage(nAccount, FD[1]);//发送给DBServer
											}
										}
										else if(nAccount.move() >= 3)
										{
											if(fd == FD[2])//从GameServer发来
											{
												SendMessage(nAccount, nAccount.fd());//发给客户端
											}
											else//从客户端发来
											{
												nAccount.set_fd(fd);
												SendMessage(nAccount, FD[2]);//发送给GateServer
											}
										}
										MessageMap[fd] = "";
										indexMap[fd] = 0;
										continue;
									}
									if(indexMap[fd] < 4)
									{
										LengthMap[fd] = LengthMap[fd] * 10 + mem->buf[i] - '0';
										indexMap[fd] ++;
									}
								}
								free(mem);
							}
							else if(_s == 0)
							{
								epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
								close(fd);
								free(mem);//释放内存
							}
							else
							{
								continue;
							}
						}
						else if(ret_ev[i].events & EPOLLOUT)
						{
                            data_buf_p mem = (data_buf_p)ret_ev[i].data.ptr;
                            int fd = mem->fd;
							ev.events = EPOLLIN;
							ev.data.fd = fd;
							epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
						}
					}
				}
			}
			break;
		}
	}
}
int main()
{
	CEpoll* epoll = new CEpoll();
	int port = atoi("1111");
	char ipp[] = "10.0.128.212";
	char *ip = ipp;
    epoll->listen_sock = epoll->start(port, ip);//epoll初始化
    epoll_servce(epoll->listen_sock);
	close(epoll->listen_sock);
}

