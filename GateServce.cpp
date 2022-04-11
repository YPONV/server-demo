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
                                        while(1)
                                        {
                                            if(SMSG->MsgQueue_Wait())
                                            {
                                                if(nAccount.move() == 1)//注册
                                                {
                                                    string str = "Register";
                                                    SMSG->que.push(str);
													SMSG->que.push(IntToString(fd));
													SMSG->que.push(nAccount.name());
													SMSG->que.push(nAccount.password());
                                                }
                                                else if(nAccount.move() == 2)//登录
                                                {
													string str = "Login";
                                                    SMSG->que.push(str);
													SMSG->que.push(IntToString(fd));
													SMSG->que.push(nAccount.uid());
													SMSG->que.push(nAccount.password());
                                                }
												SMSG->MsgQueue_Close();
                                            }
											else sleep(0.03);
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
void SendToPlayer(Account& nAccount,int fd, string flag)//flag记录是什么事件
{
	char p[1024],pp[1024];
	memset(p,'\0',sizeof p);
	memset(pp,'\0',sizeof pp);
	int sz = nAccount.ByteSize();
	nAccount.SerializeToArray(p, sz);
	AddPack(pp, p, sz);
	write(fd, pp, sz + 4);
	if(nAccount.flag() == "YES" && flag == "Login")//将登录成功的消息发送给gameserve
	{
		SharedMemory* SendMessage = new SharedMemory();
    	SendMessage->SharedMemory_init(0x5005, 1024, 0640|IPC_CREAT, 0x5006, 1, 0640);
		while(1)
		{
			if(SendMessage->SharedMemory_Wait())
            {
				string str1 = SendMessage->SharedMemory_Read();
                if(str1 == "OK")//可以继续发送
                {
					str1 = nAccount.uid();
					cout<<str1<<endl;
					SendMessage->SharedMemory_Write(str1);
					SendMessage->SharedMemory_Close();
					break;
				}
				SendMessage->SharedMemory_Close();
			}
			else sleep(0.03);
		}
	}
}
static void * HandleMessage(void *arg)
{
	int index = 0, fd;
	string flag = "";
	Account nAccount;
	while(1)
	{
		if(RMSG->MsgQueue_Wait())
		{
			while(!RMSG->que.empty())
			{
				string str = RMSG->que.front();
                index ++;
                RMSG->que.pop();
				if(index == 1)
				{
					flag = str;
				}
				else if(index == 2)
				{
					fd = StringToInt(str);
				}
				else if(index == 3)
				{
					nAccount.set_uid(str);
				}
				else if(index == 4)
				{
					nAccount.set_flag(str);
					SendToPlayer(nAccount, fd, flag);
				}
			}
			RMSG->MsgQueue_Close();
		}
		else sleep(0.03);
	}
}
static void * Rpthread(void *arg)//接收来自DBServce的消息
{
    SharedMemory* SendMessage = new SharedMemory();
    SendMessage->SharedMemory_init(0x5003, 1024, 0640|IPC_CREAT, 0x5004, 1, 0640);
    while(1)
    {
        if(RMSG->MsgQueue_Wait())
        {
            if(SendMessage->SharedMemory_Wait())
            {
                string str1 = SendMessage->SharedMemory_Read();
                if(str1 != "OK")//是发送过来的消息
                {
                    RMSG->que.push(str1);  
                    str1 = "OK";
                    SendMessage->SharedMemory_Write(str1);
                }
                SendMessage->SharedMemory_Close();
            }
            RMSG->MsgQueue_Close();
        }
        sleep(0.03);
    }
}
static void * Spthread(void *arg)//给DBServce发送消息
{
    SharedMemory* SendMessage = new SharedMemory();
    SendMessage->SharedMemory_init(0x5001, 1024, 0640|IPC_CREAT, 0x5002, 1, 0640);
    while(1)
    {
        if(SMSG->MsgQueue_Wait())
        {
            if(SendMessage->SharedMemory_Wait())
            {
                string str1 = SendMessage->SharedMemory_Read();
                if(str1 == "OK")//可以继续发送
                {
                    if(!SMSG->que.empty())
                    {
                        string str = SMSG->que.front();
                        SMSG->que.pop();
                        SendMessage->SharedMemory_Write(str);//将信息写入
                    }
                }
                SendMessage->SharedMemory_Close();
            }
            SMSG->MsgQueue_Close();
        }
        sleep(0.03);
    }
}
int main()
{
    SMSG = new MsgQueue();
    RMSG = new MsgQueue();
    SMSG->MsgQueue_Init();
    RMSG->MsgQueue_Init();
    pthread_t tidp[3];
    if ((pthread_create(&tidp[0], NULL, Spthread, NULL)) == -1)
	{
		printf("create 1 error!\n");
	}
    if ((pthread_create(&tidp[1], NULL, Rpthread, NULL)) == -1)
	{
		printf("create 2 error!\n");
	}
	if ((pthread_create(&tidp[2], NULL, HandleMessage, NULL)) == -1)
	{
		printf("create 3 error!\n");
	}
	if(SMSG->MsgQueue_Wait())
	{
		string ff;
		string str = "Login";
		SMSG->que.push(str);
		SMSG->que.push(IntToString(10));
		ff = "11111111";
		SMSG->que.push(ff);
		ff = "123456";
		SMSG->que.push(ff);
		SMSG->MsgQueue_Close();
	}
	CEpoll* epoll = new CEpoll();
	int port = atoi("8080");
	char ipp[] = "10.0.128.212";
	char *ip = ipp;
    epoll->listen_sock = epoll->start(port, ip);//epoll初始化
    epoll_servce(epoll->listen_sock);
	close(epoll->listen_sock);
}