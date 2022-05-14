#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string.h>
#include <queue>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <sys/shm.h>
#include "project.pb.h"
#include "MsgQueue.h"
#include "CSql.h"
#include "CRedis.h"
#include "CEpoll.h"
#include "BitMap.h"
using namespace std;
MsgQueue* RMSG;//接收消息
BitMap* bitmap;
CRedis* redis;
CSql* sql;
map<int, int> LengthMap;//通过fd获取剩余需要读取的消息长度
map<int, string> MessageMap;//通过fd得到缓存的消息
map<int, int> indexMap;//通过fd得知目前消息头读到了第几位
typedef struct _data_buf
{
	int fd;
	char buf[_BUF_SIZE_];
}data_buf_t,*data_buf_p;
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
		num /= 10;
	}
	reverse(str.begin(),str.end());
	return str;
}
int StringToInt(string str)
{
	int number = 0;
	for(int i = 0; i < str.size(); i ++ )
	{
		number = number * 10 + str[i] - '0';
	}
	return number;
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
long long Gettime()
{
    struct timeval tv;
	gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
void SendToGateServer(Account& nAccount, int sockfd)//flag记录是什么事件
{
	char p[1024],pp[1024];
	memset(p,'\0',sizeof p);
	memset(pp,'\0',sizeof pp);
	int sz = nAccount.ByteSize();
	nAccount.SerializeToArray(p, sz);
	AddPack(pp, p, sz);
	char* ptr = pp;
	sz += 4;
	long long T = Gettime();
	while(sz > 0)
	{
		long long now = Gettime();
		if(now - T > 2)break;
		int written_bytes = write(sockfd, ptr, sz);
		if(written_bytes < 0)
        {       
			cout<<nAccount.flag()<<endl;
            printf("SendMessage error!\n");
        }
        sz -= written_bytes;
        ptr += written_bytes;     
	}
}
bool Isexist(string str)
{
    string password = "";//用来存储查到的密码
    int ret = redis->Redis_Query(str, password);
    if(password == "")
    {
        sql->Sql_Query(str, password);
        if(password == "")
        {
            return true;
        }
        else return false;
    }
    else return false;
}
void Player_check(Account& nAccount, int sockfd)
{
    int flag = 0;
    string password = "";//用来存储查到的密码
    string UID = nAccount.uid();
    int ret = redis->Redis_Query(UID, password);
    if(password == "")//redis未查到密码，查mysql
    {
		if(bitmap->IsExist(UID))//过滤器中存在
		{
			flag = sql->Sql_Query(UID, password);
			if(flag == 1 && password == nAccount.password())//mysql内有,存入Redis
			{
				redis->Redis_Write(UID, password);
			}
		}
    }
    if(password == nAccount.password())
    {
        nAccount.set_flag(true);
    }
    else
    {
        nAccount.set_flag(false);
    }
    SendToGateServer(nAccount, sockfd);
}
void Player_register(Account& nAccount, int sockfd)
{
    string str;
    srand(unsigned(time(nullptr)));
    while(1)
    {
        str="";
        for(int i = 1; i <= 8; i ++ )
        {
            int random = rand() % 10;
            str += random + '0';
        }
        if(Isexist(str))//账号合法
        {
			nAccount.set_flag(true);
			nAccount.set_uid(str);
			string name = nAccount.name();
			string password = nAccount.password();
            sql->Sql_Write(str, name, password);
			redis->Redis_Write(str, password);
			bitmap->Set(str);
			SendToGateServer(nAccount, sockfd);
            break;
        }
        else sleep(0.003);
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
						ev.events = EPOLLIN;
						ev.data.fd = new_sock;
                        LengthMap[new_sock] = 0;
                        MessageMap[new_sock] = "";
                        indexMap[new_sock] = 0;
						epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_sock, &ev);
						printf("get a new client\n");
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
										cout<<"YES"<<endl;
										for(int j = 0; j < MessageMap[fd].size(); j ++){
											p[j] = MessageMap[fd][j];
										}
										Account nAccount;
										nAccount.ParseFromArray(p, MessageMap[fd].size());
										if(nAccount.move() == 1)
										{
											Player_check(nAccount, fd);
										}
										else if(nAccount.move() == 2)
										{
											Player_check(nAccount, fd);
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
							}
							else if(_s == 0)
							{
								epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
								close(fd);
								cout<<"lost"<<endl;
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
	redis = new CRedis();
    redis->Redis_Connect();
    sql = new CSql();
    sql->Sql_Connect();
	bitmap = new BitMap(10000010);//布隆过滤器初始化
	string str = sql->GetAllID();//拿过来的所有账号以-隔开
	bitmap->Init(str);//初始化
    CEpoll* epoll = new CEpoll();
	int port = atoi("2222");
	char ipp[] = "10.0.128.212";
	char *ip = ipp;
    epoll->listen_sock = epoll->start(port, ip);//epoll初始化
    epoll_servce(epoll->listen_sock);
	close(epoll->listen_sock);
}
